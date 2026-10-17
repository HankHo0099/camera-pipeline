#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <utility>
#include <condition_variable>
std::condition_variable not_empty;
bool producer_done = false;

struct Frame {
    int id;
    cv::Mat image;
    std::chrono::steady_clock::time_point timestamp;
};

std::queue<Frame> buffer;
std::mutex mtx;

const size_t MAX_BUFFER_SIZE = 5;

int produced_count = 0;
int consumed_count = 0;
int dropped_count = 0;
double total_decode_ms = 0.0;
double total_clone_ms = 0.0;
double total_process_ms = 0.0;  // Consumer 使用

void producer() {
    cv::VideoCapture cap("data/test.mp4");

    if (!cap.isOpened()) {
        std::cerr << "Failed to open video!" << std::endl;
        return;
    }

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
        if (buffer.size() >= MAX_BUFFER_SIZE) {
            buffer.pop();
            dropped_count++;
        }

        buffer.push(std::move(frame));
    }
    not_empty.notify_one();
    }

    std::cout << "Producer finished. Frames: "
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

        cv::Mat gray;
        cv::cvtColor(frame.image, gray, cv::COLOR_BGR2GRAY);
        auto process_end = std::chrono::steady_clock::now();

        total_process_ms +=
            std::chrono::duration<double, std::milli>(
                process_end - process_start
            ).count();
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
    std::cout << "Average decode time: "
          << total_decode_ms / produced_count
          << " ms/frame" << std::endl;
    std::cout << "Average clone time: "
            << total_clone_ms / produced_count << " ms/frame\n";

    std::cout << "Average process time: "
            << total_process_ms / consumed_count << " ms/frame\n";
    auto end = std::chrono::steady_clock::now();
    std::cout << "\n===== Pipeline Statistics =====\n";
    std::cout << "Produced: " << produced_count << std::endl;
    std::cout << "Consumed: " << consumed_count << std::endl;
    std::cout << "Dropped: " << dropped_count << std::endl;
    std::cout << "Remaining: " << buffer.size() << std::endl;
    std::cout << "Pipeline finished." << std::endl;
    double elapsed_sec =
        std::chrono::duration<double>(end - start).count();

    double pipeline_fps = consumed_count / elapsed_sec;

    std::cout << "Elapsed time: "
            << elapsed_sec << " sec" << std::endl;

    std::cout << "Pipeline FPS: "
            << pipeline_fps << std::endl;
    return 0;
}