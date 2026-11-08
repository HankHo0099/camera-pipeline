#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>

int main() {
    cv::VideoCapture cap("data/test.mp4");

    if (!cap.isOpened()) {
        std::cerr << "Failed to open video!\n";
        return 1;
    }

    cv::Mat image;
    int frame_count = 0;

    auto start = std::chrono::steady_clock::now();

    while (cap.read(image)) {
        cv::Mat copied = image.clone();

        cv::Mat gray;
        cv::cvtColor(copied, gray, cv::COLOR_BGR2GRAY);

        frame_count++;
    }

    auto end = std::chrono::steady_clock::now();

    double elapsed_sec =
        std::chrono::duration<double>(end - start).count();

    std::cout << "Frames: " << frame_count << '\n';
    std::cout << "Elapsed: " << elapsed_sec << " sec\n";
    std::cout << "FPS: " << frame_count / elapsed_sec << '\n';

    return 0;
}