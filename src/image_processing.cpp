#include "image_processing.h"
#include <cmath>
#include <algorithm>
// Local working buffers allow independent frames to be processed concurrently.

void grayscale(const cv::Mat& input, cv::Mat& output) {
    output.create(input.rows, input.cols, CV_8UC1);
    for(int row=0; row<input.rows; ++row){
        for(int col=0; col<input.cols; ++col){
            cv::Vec3b pixel = input.at<cv::Vec3b>(row, col);
            uchar b = pixel[0];
            uchar g = pixel[1];
            uchar r = pixel[2];
            uchar gray_value = 0.114*b + 0.587*g + 0.299*r;
            output.at<uchar>(row, col) = gray_value;
        }
    }

}
void gaussianBlur(const cv::Mat& input, cv::Mat& output) {

    // Apply the 3x3 kernel to interior pixels; the one-pixel border stays zero.
    output = cv::Mat::zeros(input.size(), CV_8UC1);

    int kernel[3][3] = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
    };

    for (int row = 1; row < input.rows - 1; ++row) {
        for (int col = 1; col < input.cols - 1; ++col) {

            int sum = 0;

            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {

                    sum += input.at<uchar>(row - 1 + i, col - 1 + j)
                           * kernel[i][j];
                }
            }

            output.at<uchar>(row, col) = sum / 16;
        }
    }
}

void sobelEdge(const cv::Mat& input, cv::Mat& output) {

    // Combine horizontal and vertical gradients; the one-pixel border stays zero.
    output = cv::Mat::zeros(input.size(), CV_8UC1);

    int kernel_x[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    int kernel_y[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    for (int row = 1; row < input.rows - 1; ++row) {
        for (int col = 1; col < input.cols - 1; ++col) {

            int gx = 0;
            int gy = 0;


            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    uchar pixel = input.at<uchar>(row - 1 + i, col - 1 + j);
                    gx += pixel * kernel_x[i][j];
                    gy += pixel * kernel_y[i][j];
                }
            }

            double magnitude = std::sqrt(gx * gx + gy * gy);
            magnitude = std::min(255.0, magnitude);
            output.at<uchar>(row, col) = static_cast<uchar>(magnitude);

        }
    }
}


void computeHistogram(const cv::Mat& input, int hist[256]) {

    for (int i = 0; i < 256; ++i) {
        hist[i] = 0;
    }

    for (int row = 0; row < input.rows; ++row) {
        for (int col = 0; col < input.cols; ++col) {

            uchar pixel = input.at<uchar>(row, col);

            hist[pixel]++;
        }
    }
}


void histogramEqualization(const cv::Mat& input, cv::Mat& output) {

    int hist[256];
    computeHistogram(input, hist);

    int cdf[256];

    // Prefix sum gives the cumulative distribution of intensity counts.
    cdf[0] = hist[0];
    for(int i=1; i<256; i++){
        cdf[i] = cdf[i-1] + hist[i];
    }

    // Find the first nonzero cumulative count.
    int cdf_min = 0;

    for (int i = 0; i < 256; i++) {
        if (cdf[i] != 0) {
            cdf_min = cdf[i];
            break;
        }
    }

    int N = input.rows * input.cols;
    uchar lut[256];

    // A constant image has no intensity range to redistribute.
    if (N == cdf_min) {
        input.copyTo(output);
        return;
    }
    // Build a clamped lookup table from the normalized CDF.
    for (int i = 0; i < 256; i++) {
        double value = (cdf[i] - cdf_min) * 255.0 / (N - cdf_min);
        lut[i] = static_cast<uchar>(std::clamp(value, 0.0, 255.0));
    }

    output.create(input.rows, input.cols, CV_8UC1);

    for (int row = 0; row < input.rows; ++row) {
        for (int col = 0; col < input.cols; ++col) {

            uchar pixel = input.at<uchar>(row, col);

            output.at<uchar>(row, col) = lut[pixel];
        }
    }
}
