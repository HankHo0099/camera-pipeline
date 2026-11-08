#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <utility>
#include <condition_variable>
#include <vector>
#include <algorithm>
std::condition_variable not_empty;
bool producer_done = false;

struct Frame {
    int id;
    cv::Mat image;
    std::chrono::steady_clock::time_point timestamp;
};

enum class DropPolicy {
    Oldest,
    Newest
};

struct PipelineConfig {
    std::string video_path = "data/test.mp4";

    int target_fps = 75;
    size_t queue_capacity = 5;
    int consumer_delay_ms = 10;

    bool enable_fps_limit = true;
    DropPolicy drop_policy = DropPolicy::Newest;
};

class BoundedQueue {
    private:
        std::queue<Frame> buffer_;
        std::mutex mtx_;
        std::condition_variable not_empty_;

        size_t capacity_;
        DropPolicy policy_;
        bool closed_ = false;

    public:
        BoundedQueue(size_t capacity, DropPolicy policy)
            : capacity_(capacity), policy_(policy) {
        }
        bool push(Frame frame) {
        {
            std::lock_guard<std::mutex> lock(mtx_);

            if (closed_) {
                return false;
            }

            if (buffer_.size() >= capacity_) {
                if (policy_ == DropPolicy::Oldest) {
                    buffer_.pop();
                } else {
                    return false;
                }
            }

            buffer_.push(std::move(frame));
        }
            not_empty_.notify_one();
            return true;
        }
        bool pop(Frame& frame) {
            std::unique_lock<std::mutex> lock(mtx_);

            not_empty_.wait(lock, [this] {
                return !buffer_.empty() || closed_;
            });

            if (buffer_.empty()) {
                return false;
            }

            frame = std::move(buffer_.front());
            buffer_.pop();

            return true;
        }
        void close() {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                closed_ = true;
            }

            not_empty_.notify_all();
        }
};

std::queue<Frame> buffer;
std::mutex mtx;
const PipelineConfig config;
const size_t MAX_BUFFER_SIZE = 5;
std::vector<double> latencies;
int produced_count = 0;
int consumed_count = 0;
int dropped_count = 0;
double total_decode_ms = 0.0;
double total_clone_ms = 0.0;
double total_process_ms = 0.0;  // Consumer 使用
double total_latency_ms = 0.0;
double max_latency_ms = 0.0;
double max_wait_ms = 0.0;
double max_process_ms = 0.0;

void producer() {

    auto frame_interval = std::chrono::duration<double>(
        1.0 / config.target_fps
    );

    cv::VideoCapture cap(config.video_path);

    if (!cap.isOpened()) {
        std::cerr << "Failed to open video!" << std::endl;

        {
            std::lock_guard<std::mutex> lock(mtx);
            producer_done = true;
        }

        not_empty.notify_one();
        return;
    }
    auto next_frame_time = std::chrono::steady_clock::now();

    cv::Mat image;
    int frame_count = 0;

    while (true) {
        auto decode_start = std::chrono::steady_clock::now();

        bool success = cap.read(image);

        auto decode_end = std::chrono::steady_clock::now();

        if (!success) {
            break;
        }

        total_decode_ms +=
        std::chrono::duration<double, std::milli>(
            decode_end - decode_start
        ).count();

        frame_count++;

        Frame frame;
        frame.id = frame_count;
        auto clone_start = std::chrono::steady_clock::now();

        frame.image = image.clone();

        auto clone_end = std::chrono::steady_clock::now();

        total_clone_ms +=
        std::chrono::duration<double, std::milli>(
            clone_end - clone_start
        ).count();

        frame.timestamp = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(mtx);
            produced_count++;
            if (buffer.size() >= config.queue_capacity) {
                if (config.drop_policy == DropPolicy::Oldest) {
                    buffer.pop();
                    buffer.push(std::move(frame));
                }

                dropped_count++;
            } else {
                buffer.push(std::move(frame));
            }
        }
        not_empty.notify_one();
            
        if (config.enable_fps_limit) {
            next_frame_time += std::chrono::duration_cast<
                std::chrono::steady_clock::duration
            >(frame_interval);

            std::this_thread::sleep_until(next_frame_time);
        }
        
    }    std::cout << "Producer finished. Frames: "
              << frame_count << std::endl;

    {
        std::lock_guard<std::mutex> lock(mtx);
        producer_done = true;
    }

    not_empty.notify_one();

}

void consumer() {
    while (true) {
        Frame frame;

        {
            std::unique_lock<std::mutex> lock(mtx);

            not_empty.wait(lock, [] {
                return !buffer.empty() || producer_done;
            });

            if (buffer.empty() && producer_done) {
                break;
            }

            frame = std::move(buffer.front());
            buffer.pop();
            consumed_count++;
        }
        auto process_start = std::chrono::steady_clock::now();
        double wait_ms =
            std::chrono::duration<double, std::milli>(
                process_start - frame.timestamp
            ).count();

        max_wait_ms = std::max(max_wait_ms, wait_ms);

        cv::Mat gray;
        cv::cvtColor(frame.image, gray, cv::COLOR_BGR2GRAY);
        if (config.consumer_delay_ms > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config.consumer_delay_ms)
            );
        }

        auto process_end = std::chrono::steady_clock::now();
        double process_ms =
            std::chrono::duration<double, std::milli>(
                process_end - process_start
            ).count();

        max_process_ms = std::max(max_process_ms, process_ms);
        total_process_ms +=
            std::chrono::duration<double, std::milli>(
                process_end - process_start
            ).count();

        double latency_ms =
        std::chrono::duration<double, std::milli>(
            process_end - frame.timestamp
        ).count();
        latencies.push_back(latency_ms);
        total_latency_ms += latency_ms;
        max_latency_ms = std::max(max_latency_ms, latency_ms);
        // std::cout << "Processed frame: "
        //           << frame.id << std::endl;
    }

    std::cout << "Consumer finished." << std::endl;
}






int main() {

    auto start = std::chrono::steady_clock::now();

    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();
    auto end = std::chrono::steady_clock::now();
    std::sort(latencies.begin(), latencies.end());

    if (!latencies.empty()) {
        size_t n = latencies.size();

        double p50 = latencies[static_cast<size_t>(0.50 * (n - 1))];
        double p95 = latencies[static_cast<size_t>(0.95 * (n - 1))];
        double p99 = latencies[static_cast<size_t>(0.99 * (n - 1))];

        std::cout << "P50 latency: " << p50 << " ms\n";
        std::cout << "P95 latency: " << p95 << " ms\n";
        std::cout << "P99 latency: " << p99 << " ms\n";
    }
    double elapsed_sec =
        std::chrono::duration<double>(end - start).count();
    std::cout << "Average decode time: "
          << total_decode_ms / produced_count
          << " ms/frame" << std::endl;
    std::cout << "Average clone time: "
            << total_clone_ms / produced_count << " ms/frame\n";
    std::cout << "Average latency: "
            << total_latency_ms / consumed_count
            << " ms/frame\n";

    std::cout << "Max latency: "
            << max_latency_ms
            << " ms\n";
    std::cout << "Max wait time: "
            << max_wait_ms << " ms\n";

    std::cout << "Max process time: "
            << max_process_ms << " ms\n";
            
    std::cout << "Average process time: "
            << total_process_ms / consumed_count << " ms/frame\n";
    std::cout << "\n===== Pipeline Statistics =====\n";
    std::cout << "Produced: " << produced_count << std::endl;
    std::cout << "Consumed: " << consumed_count << std::endl;
    std::cout << "Dropped: " << dropped_count << std::endl;
    std::cout << "Remaining: " << buffer.size() << std::endl;
    std::cout << "Pipeline finished." << std::endl;


    double pipeline_fps = consumed_count / elapsed_sec;

    std::cout << "Elapsed time: "
            << elapsed_sec << " sec" << std::endl;

    std::cout << "Pipeline FPS: "
            << pipeline_fps << std::endl;
    return 0;
}