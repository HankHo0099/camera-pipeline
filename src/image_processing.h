#pragma once

#include <opencv2/core.hpp>

// Use separate input/output Mats for the image-processing functions.
// Input: CV_8UC3 BGR image. Output: same-size CV_8UC1 grayscale image.
// Convert each BGR pixel to a grayscale intensity.
void grayscale(const cv::Mat& input, cv::Mat& output);

// Input: CV_8UC1 grayscale image. Output: same-size CV_8UC1 blurred image.
// Smooth the image with a Gaussian kernel to reduce noise.
void gaussianBlur(const cv::Mat& input, cv::Mat& output);

// Input: CV_8UC1 grayscale image. Output: same-size CV_8UC1 edge image.
// Compute horizontal and vertical Sobel gradients and map edge strength to 0-255.
void sobelEdge(const cv::Mat& input, cv::Mat& output);

// Input: CV_8UC1 image. Resets and fills all 256 intensity counts in hist.
void computeHistogram(const cv::Mat& input,int hist[256]);

// Input: CV_8UC1 image. Output: same-size CV_8UC1 CDF-equalized image.
// Calls computeHistogram internally; constant images are copied unchanged.
void histogramEqualization(const cv::Mat& input, cv::Mat& output);
