#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <chrono>
#include <opencv2/imgproc.hpp>

int main() {
    cv::VideoCapture cap("data/test.mp4");

    if (!cap.isOpened()) {
        std::cerr << "Failed to open video." << std::endl;
        return 1;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    std::cout << "Resolution: " << width << " x " << height << std::endl;
    std::cout << "FPS: " << fps << std::endl;

    cv::Mat frame;
    cv::Mat gray;
    int frame_count = 0;

    auto start = std::chrono::steady_clock::now();

    while (cap.read(frame)) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        frame_count++;

        if (frame_count % 100 == 0) {
            std::cout << "Processed " << frame_count << " frames" << std::endl;
        }
    }

    auto end = std::chrono::steady_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    double processing_fps = frame_count / elapsed;

    std::cout << "Elapsed time: " << elapsed << " sec" << std::endl;
    std::cout << "Processing FPS: " << processing_fps << std::endl;

    std::cout << "Finished." << std::endl;
    std::cout << "Total frames: " << frame_count << std::endl;

    return 0;

}
