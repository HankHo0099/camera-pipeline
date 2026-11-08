#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>

struct Frame {
    int id;
    std::chrono::steady_clock::time_point timestamp;
};

std::queue<Frame> buffer;
std::mutex mtx;

std::condition_variable not_empty;
std::condition_variable not_full;
bool producer_done = false;
int produced_count = 0;
int consumed_count = 0;
int dropped_count = 0;
const size_t MAX_BUFFER_SIZE = 5;

void producer() {
    for (int i = 1; i <= 100; i++) {

        {
            std::lock_guard<std::mutex> lock(mtx);

            produced_count++;

            if (buffer.size() >= MAX_BUFFER_SIZE) {
                std::cout << "Dropped newest: " << i << std::endl;
                dropped_count++;
            } else {
                Frame frame;
                frame.id = i;
                frame.timestamp = std::chrono::steady_clock::now();
                buffer.push(frame);
            }

            std::cout << "Buffer size: "
                      << buffer.size() << std::endl;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(33)
        );
        std::cout << "Produced: " << i << std::endl;
    }
        {
        std::lock_guard<std::mutex> lock(mtx);
        producer_done = true;
        }
        not_empty.notify_one();
}

void consumer() {
    while (true) {
        Frame data;
        double waiting_ms; 

        {
            std::unique_lock<std::mutex> lock(mtx);

            not_empty.wait(lock, [] {
                return !buffer.empty() || producer_done;
            });

            if (buffer.empty() && producer_done) {
                break;
            }

            data = buffer.front();
            buffer.pop();
            consumed_count++;
            
            auto dequeue_time = std::chrono::steady_clock::now();

            waiting_ms =
                std::chrono::duration<double, std::milli>(
                    dequeue_time - data.timestamp
                ).count();
                
            
        }

        std::cout << "Consumed: " << data.id
                  << " | Queue wait: " << waiting_ms
                  << " ms" << std::endl;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(500)
        );
    }

    std::cout << "Consumer finished." << std::endl;
}


int main() {
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    std::cout << "\n===== Pipeline Statistics =====\n";
    std::cout << "Produced: " << produced_count << std::endl;
    std::cout << "Consumed: " << consumed_count << std::endl;
    std::cout << "Dropped: " << dropped_count << std::endl;

    std::cout << "Remaining: " << buffer.size() << std::endl;

    return 0;
}