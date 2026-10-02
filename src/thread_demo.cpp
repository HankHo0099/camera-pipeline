#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
std::queue<int> buffer;
std::mutex mtx;

std::condition_variable not_empty;
std::condition_variable not_full;

const size_t MAX_BUFFER_SIZE = 5;

void producer() {
    for (int i = 1; i <= 10; i++) {

        {
            std::unique_lock<std::mutex> lock(mtx);

            not_full.wait(lock, [] {
                return buffer.size() < MAX_BUFFER_SIZE;
            });

            buffer.push(i);
        }

        not_empty.notify_one();

        std::cout << "Produced: " << i << std::endl;
    }
}

void consumer() {
    for (int i = 1; i <= 10; i++) {
        int data;

        {
            std::unique_lock<std::mutex> lock(mtx);

            not_empty.wait(lock, [] {
                return !buffer.empty();
            });

            data = buffer.front();
            buffer.pop();

        }

        not_full.notify_one();

        std::cout << "Consumed: " << data << std::endl;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(500)
        );


    }
}

int main() {
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    return 0;
}