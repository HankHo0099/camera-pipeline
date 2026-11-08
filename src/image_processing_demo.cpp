#include "image_processing.h"

#include <iostream>
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

int main() {
    // Run from the project root so this relative path finds the test video.
    cv::VideoCapture cap("data/test.mp4");
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video.\n";
        return 1;
    }

    cv::Mat frame;
    if (!cap.read(frame)) {
        std::cerr << "Failed to read the first frame.\n";
        return 1;
    }
    std::cout << "First frame: " << frame.cols << " x " << frame.rows << '\n';

    cv::Mat gray;
    cv::Mat blurred;
    cv::Mat edges;
    cv::Mat equalized;

    // Use the same custom stages as the benchmark on the first decoded frame.
    grayscale(frame, gray);
    gaussianBlur(gray, blurred);
    sobelEdge(blurred, edges);
    histogramEqualization(edges, equalized);

    std::filesystem::create_directories("results");
    if (!cv::imwrite("results/original.png", frame) ||
        !cv::imwrite("results/grayscale.png", gray) ||
        !cv::imwrite("results/gaussian.png", blurred) ||
        !cv::imwrite("results/sobel.png", edges) ||
        !cv::imwrite("results/equalized.png", equalized)) {
        std::cerr << "Failed to write PNG results.\n";
        return 1;
    }

    std::cout << "Saved all five processing stages to results/.\n";
    return 0;
}
