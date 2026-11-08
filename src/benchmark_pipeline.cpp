#include "image_processing.h"

#include <opencv2/videoio.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

// Run every custom stage; discard the output only after equalization completes.
void processFrame(const cv::Mat& frame) {
    cv::Mat gray, blurred, edges, equalized;
    grayscale(frame, gray);
    gaussianBlur(gray, blurred);
    sobelEdge(blurred, edges);
    histogramEqualization(edges, equalized);
}

struct Result {
    std::uint64_t total = 0;
    std::uint64_t processed = 0;
    double seconds = 0;
};

// Measure complete video reading without image processing or a frame queue.
Result runDecodeOnly(const std::string& path) {
    Result result;
    const auto start = Clock::now();
    cv::VideoCapture capture(path);
    if (!capture.isOpened()) {
        throw std::runtime_error("Failed to open video: " + path);
    }
    cv::Mat frame;
    while (capture.read(frame)) {
        ++result.total;
    }
    result.seconds = std::chrono::duration<double>(Clock::now() - start).count();
    return result;
}

// Read and fully process each frame before reading the next one.
Result runSingle(const std::string& path) {
    Result result;
    const auto start = Clock::now();
    cv::VideoCapture capture(path);
    if (!capture.isOpened()) {
        throw std::runtime_error("Failed to open video: " + path);
    }
    cv::Mat frame;
    while (capture.read(frame)) {
        ++result.total;
        processFrame(frame);
        ++result.processed;
    }
    result.seconds = std::chrono::duration<double>(Clock::now() - start).count();
    return result;
}

// Read on the calling thread while fixed workers process independent frames.
Result runWorkers(const std::string& path, int workerCount) {
    Result result;
    const auto start = Clock::now();
    cv::VideoCapture capture(path);
    if (!capture.isOpened()) {
        throw std::runtime_error("Failed to open video: " + path);
    }

    constexpr std::size_t capacity = 16;
    std::queue<cv::Mat> frames;
    std::mutex mutex;
    std::condition_variable notEmpty, notFull;
    bool finished = false;
    std::vector<std::uint64_t> processed(workerCount, 0);
    std::vector<std::thread> workers;
    for (int i = 0; i < workerCount; ++i) {
        workers.emplace_back([&, i] {
            while (true) {
                cv::Mat frame;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    // Predicate handles both spurious wakeups and producer EOF.
                    notEmpty.wait(lock, [&] { return finished || !frames.empty(); });
                    // EOF only ends a worker after the queue has drained.
                    if (frames.empty()) {
                        break;
                    }
                    frame = std::move(frames.front());
                    frames.pop();
                }
                notFull.notify_one();
                // Processing happens outside the lock; buffers belong to this worker.
                processFrame(frame);
                ++processed[i];
            }
        });
    }

    // The calling thread is the producer and the only owner of VideoCapture.
    while (true) {
        cv::Mat frame;
        if (!capture.read(frame)) {
            break;
        }
        ++result.total;
        {
            std::unique_lock<std::mutex> lock(mutex);
            // A full queue blocks the producer instead of dropping a frame.
            notFull.wait(lock, [&] { return frames.size() < capacity; });
            // A fresh Mat on each read keeps queued frame storage independent.
            frames.push(std::move(frame));
        }
        notEmpty.notify_one();
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        finished = true;
    }
    notEmpty.notify_all();
    for (auto& worker : workers) {
        worker.join();
    }
    result.seconds = std::chrono::duration<double>(Clock::now() - start).count();
    // Join synchronizes access to each worker's private counter.
    for (auto count : processed) {
        result.processed += count;
    }
    return result;
}

// Print end-to-end throughput and speedup relative to the sequential run.
void printResult(const Result& result, int workers, double baselineSeconds) {
    std::cout << std::left << std::setw(18)
              << (workers == 0 ? "Single-thread" : "Multithreading")
              << std::right << std::setw(8) << workers
              << std::setw(14) << result.total
              << std::setw(14) << result.processed
              << std::fixed << std::setprecision(3)
              << std::setw(14) << result.seconds
              << std::setw(14) << result.processed / result.seconds
              << std::setw(12) << baselineSeconds / result.seconds << std::endl;
}
} // namespace

int main(int argc, char* argv[]) {
    const std::string path = argc > 1 ? argv[1] : "data/test.mp4";
    // Keep OpenCV processing single-threaded; parallelism here is across frames.
    cv::setNumThreads(1);
    try {
        std::cout << "Video: " << path << "\nQueue capacity: 16\n"
                  << std::left << std::setw(18) << "Mode" << std::right
                  << std::setw(8) << "Workers" << std::setw(14) << "Total Frames"
                  << std::setw(14) << "Processed" << std::setw(14) << "Elapsed (s)"
                  << std::setw(14) << "FPS" << std::setw(12) << "Speedup"
                  << std::endl;
        const auto decode = runDecodeOnly(path);
        // Decode-only has no image processing or processing speedup to report.
        std::cout << std::left << std::setw(18) << "Decode-only" << std::right
                  << std::setw(8) << 0 << std::setw(14) << decode.total
                  << std::setw(14) << "N/A"
                  << std::fixed << std::setprecision(3)
                  << std::setw(14) << decode.seconds
                  << std::setw(14) << decode.total / decode.seconds
                  << std::setw(12) << "N/A" << std::endl;
        const auto baseline = runSingle(path);
        printResult(baseline, 0, baseline.seconds);
        if (baseline.total == 0 || baseline.total != baseline.processed) {
            throw std::runtime_error("Baseline frame count check failed.");
        }
        for (int workers : {1, 2, 4, 8}) {
            const auto result = runWorkers(path, workers);
            printResult(result, workers, baseline.seconds);
            if (result.total != baseline.total || result.processed != result.total) {
                throw std::runtime_error("Frame count mismatch.");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
