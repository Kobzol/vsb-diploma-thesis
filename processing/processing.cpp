#include "processing.h"
#include "../utils/utils.h"
#include "../objdetect/hasher.h"
#include <cassert>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <opencv/cv.hpp>

const uchar Processing::NORMAL_LUT[Processing::NORMAL_LUT_SIZE][Processing::NORMAL_LUT_SIZE] = {
        {32, 32, 32, 32, 32, 32, 64, 64, 64, 64, 64, 64,  64,  64,  64,  128, 128, 128, 128, 128},
        {32, 32, 32, 32, 32, 32, 32, 64, 64, 64, 64, 64,  64,  64,  128, 128, 128, 128, 128, 128},
        {32, 32, 32, 32, 32, 32, 32, 64, 64, 64, 64, 64,  64,  64,  128, 128, 128, 128, 128, 128},
        {32, 32, 32, 32, 32, 32, 32, 32, 64, 64, 64, 64,  64,  128, 128, 128, 128, 128, 128, 128},
        {32, 32, 32, 32, 32, 32, 32, 32, 64, 64, 64, 64,  64,  128, 128, 128, 128, 128, 128, 128},
        {32, 32, 32, 32, 32, 32, 32, 32, 64, 64, 64, 64,  64,  128, 128, 128, 128, 128, 128, 128},
        {16, 32, 32, 32, 32, 32, 32, 32, 32, 64, 64, 64,  128, 128, 128, 128, 128, 128, 128, 128},
        {16, 16, 16, 32, 32, 32, 32, 32, 32, 64, 64, 64,  128, 128, 128, 128, 128, 128, 1,   1},
        {16, 16, 16, 16, 16, 16, 32, 32, 32, 32, 64, 128, 128, 128, 128, 1,   1,   1,   1,   1},
        {16, 16, 16, 16, 16, 16, 16, 16, 32, 32, 64, 128, 128, 1,   1,   1,   1,   1,   1,   1},
        {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 1,  1,   1,   1,   1,   1,   1,   1,   1,   1},
        {16, 16, 16, 16, 16, 16, 16, 16, 8,  8,  4,  2,   2,   1,   1,   1,   1,   1,   1,   1},
        {16, 16, 16, 16, 16, 16, 8,  8,  8,  8,  4,  2,   2,   2,   2,   1,   1,   1,   1,   1},
        {16, 16, 16, 8,  8,  8,  8,  8,  8,  4,  4,  4,   2,   2,   2,   2,   2,   2,   1,   1},
        {16, 8,  8,  8,  8,  8,  8,  8,  8,  4,  4,  4,   2,   2,   2,   2,   2,   2,   2,   2},
        {8,  8,  8,  8,  8,  8,  8,  8,  4,  4,  4,  4,   4,   2,   2,   2,   2,   2,   2,   2},
        {8,  8,  8,  8,  8,  8,  8,  8,  4,  4,  4,  4,   4,   2,   2,   2,   2,   2,   2,   2},
        {8,  8,  8,  8,  8,  8,  8,  8,  4,  4,  4,  4,   4,   2,   2,   2,   2,   2,   2,   2},
        {8,  8,  8,  8,  8,  8,  8,  4,  4,  4,  4,  4,   4,   4,   2,   2,   2,   2,   2,   2},
        {8,  8,  8,  8,  8,  8,  8,  4,  4,  4,  4,  4,   4,   4,   2,   2,   2,   2,   2,   2}
};

void Processing::accumulateBilateral(long delta, long xShift, long yShift, long *A, long *b, int maxDifference) {
    long f = std::abs(delta) < maxDifference ? 1 : 0;

    const long fx = f * xShift;
    const long fy = f * yShift;

    A[0] += fx * xShift;
    A[1] += fx * yShift;
    A[2] += fy * yShift;
    b[0] += fx * delta;
    b[1] += fy * delta;
}

void Processing::quantizedNormals(const cv::Mat &src, cv::Mat &dst, float fx, float fy, int maxDistance, int maxDifference) {
    assert(src.type() == CV_16U); // 16-bit depth image

    dst = cv::Mat::zeros(src.size(), CV_8U);
    const int PS = 5; // patch size

    const int offsetX = Processing::NORMAL_LUT_SIZE / 2;
    const int offsetY = Processing::NORMAL_LUT_SIZE / 2;

    for (int y = PS; y < src.rows - PS; y++) {
        for (int x = PS; x < src.cols - PS; x++) {
            // Get depth value at (x,y)
            long d = src.at<ushort>(y, x);

            if (d < maxDistance) {
                long A[3], b[2];
                A[0] = A[1] = A[2] = 0;
                b[0] = b[1] = 0;

                // Get 8 points around computing points in defined patch of size PS
                accumulateBilateral(src.at<ushort>(y - PS, x - PS) - d, -PS, -PS, A, b, maxDifference);
                accumulateBilateral(src.at<ushort>(y - PS, x) - d, 0, -PS, A, b, maxDifference);
                accumulateBilateral(src.at<ushort>(y - PS, x + PS) - d, +PS, -PS, A, b, maxDifference);
                accumulateBilateral(src.at<ushort>(y, x - PS) - d, -PS, 0, A, b, maxDifference);
                accumulateBilateral(src.at<ushort>(y, x + PS) - d, +PS, 0, A, b, maxDifference);
                accumulateBilateral(src.at<ushort>(y + PS, x - PS) - d, -PS, +PS, A, b, maxDifference);
                accumulateBilateral(src.at<ushort>(y + PS, x) - d, 0, +PS, A, b, maxDifference);
                accumulateBilateral(src.at<ushort>(y + PS, x + PS) - d, +PS, +PS, A, b, maxDifference);

                // Solve
                long det = A[0] * A[2] - A[1] * A[1];
                long Dx = A[2] * b[0] - A[1] * b[1];
                long Dy = -A[1] * b[0] + A[0] * b[1];

                // Multiply differences by focal length
                float Nx = fx * Dx;
                float Ny = fy * Dy;
                auto Nz = static_cast<float>(-det * d);

                // Get normal vector size
                float norm = std::sqrt(Nx * Nx + Ny * Ny + Nz * Nz);

                if (norm > 0) {
                    float normInv = 1.0f / (norm);

                    // Normalize normal
                    Nx *= normInv;
                    Ny *= normInv;
                    Nz *= normInv;

                    // Get values for pre-generated Normal look up table
                    auto vX = static_cast<int>(Nx * offsetX + offsetX);
                    auto vY = static_cast<int>(Ny * offsetY + offsetY);
                    auto vZ = static_cast<int>(Nz * Processing::NORMAL_LUT_SIZE + Processing::NORMAL_LUT_SIZE);

                    // Save quantized normals, ignore vZ, we quantize only in top half of sphere (cone)
                    dst.at<uchar>(y, x) = NORMAL_LUT[vY][vX];
                    // dst.at<uchar>(y, x) = static_cast<uchar>(std::fabs(Nz) * 255); // Lambert
                } else {
                    dst.at<uchar>(y, x) = 0; // Discard shadows & distant objects from depth sensor
                }
            } else {
                dst.at<uchar>(y, x) = 0; // Wrong depth
            }
        }
    }

    cv::medianBlur(dst, dst, 5);
}

void Processing::filterSobel(const cv::Mat &src, cv::Mat &dst, bool xFilter, bool yFilter) {
    assert(!src.empty());
    assert(src.type() == CV_32FC1);

    const int filterX[9] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
    const int filterY[9] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};

    if (dst.empty()) {
        dst = cv::Mat(src.size(), src.type());
    }

    // Blur image little bit to reduce noise
    cv::GaussianBlur(src, dst, cv::Size(3, 3), 0, 0);

    #pragma omp parallel for default(none) shared(src, dst, filterX, filterY) firstprivate(xFilter, yFilter)
    for (int y = 1; y < src.rows - 1; y++) {
        for (int x = 1; x < src.cols - 1; x++) {
            int i = 0;
            float sumX = 0, sumY = 0;

            for (int yy = 0; yy < 3; yy++) {
                for (int xx = 0; xx < 3; xx++) {
                    float px = src.at<float>(yy + y - 1, x + xx - 1);

                    if (xFilter) { sumX += px * filterX[i]; }
                    if (yFilter) { sumY += px * filterY[i]; }

                    i++;
                }
            }

            dst.at<float>(y, x) = std::sqrt(SQR(sumX) + SQR(sumY));
        }
    }
}

void Processing::thresholdMinMax(const cv::Mat &src, cv::Mat &dst, float min, float max) {
    assert(!src.empty());
    assert(!dst.empty());
    assert(src.type() == CV_32FC1);
    assert(dst.type() == CV_32FC1);
    assert(min >= 0);
    assert(max >= 0 && max > min);

    // Apply very simple min/max thresholding for the source image
    #pragma omp parallel for default(none) firstprivate(min, max) shared(dst, src)
    for (int y = 0; y < src.rows; y++) {
        for (int x = 0; x < src.cols; x++) {
            if (src.at<float>(y, x) >= min && src.at<float>(y, x) <= max) {
                dst.at<float>(y, x) = 1.0f;
            } else {
                dst.at<float>(y, x) = 0.0f;
            }
        }
    }
}

void Processing::quantizedSurfaceNormals(const cv::Mat &srcDepth, cv::Mat &quantizedSurfaceNormals) {
    assert(!srcDepth.empty());
    assert(srcDepth.type() == CV_32FC1);

    // Blur image to reduce noise
    cv::Mat srcBlurred;
    cv::GaussianBlur(srcDepth, srcBlurred, cv::Size(3, 3), 0, 0);
//    srcBlurred = srcDepth.clone();
    quantizedSurfaceNormals = cv::Mat::zeros(srcDepth.size(), CV_8UC1);

    #pragma omp parallel for default(none) shared(srcBlurred, quantizedSurfaceNormals)
    for (int y = 1; y < srcBlurred.rows - 1; y++) {
        for (int x = 1; x < srcBlurred.cols - 1; x++) {
            float dzdx = (srcBlurred.at<float>(y, x + 1) - srcBlurred.at<float>(y, x - 1)) / 2.0f;
            float dzdy = (srcBlurred.at<float>(y + 1, x) - srcBlurred.at<float>(y - 1, x)) / 2.0f;
            cv::Vec3f d(-dzdy, -dzdx, 1.0f);

            // Normalize and save normal
            quantizedSurfaceNormals.at<uchar>(y, x) = quantizeSurfaceNormal(cv::normalize(d));
        }
    }
}

void Processing::quantizedOrientationGradients(const cv::Mat &srcGray, cv::Mat &quantizedOrientations, cv::Mat &magnitude) {
    // Checks
    assert(!srcGray.empty());
    assert(srcGray.type() == CV_32FC1);

    // Calc sobel
    cv::Mat sobelX, sobelY, angles;
    cv::Sobel(srcGray, sobelX, CV_32F, 1, 0, 3, 1, 0);
    cv::Sobel(srcGray, sobelY, CV_32F, 0, 1, 3, 1, 0);

    // Calc orientationGradients
    cv::cartToPolar(sobelX, sobelY, magnitude, angles, true);

    // Quantize orientations
    quantizedOrientations = cv::Mat(angles.size(), CV_8UC1);

    #pragma omp parallel for default(none) shared(quantizedOrientations, angles)
    for (int y = 0; y < angles.rows; y++) {
        for (int x = 0; x < angles.cols; x++) {
            quantizedOrientations.at<uchar>(y, x) = quantizeOrientationGradient(angles.at<float>(y, x));
        }
    }
}

uchar Processing::quantizeDepth(float depth, std::vector<cv::Range> &ranges) {
    // Depth should have max value of <-65536, +65536>
    assert(depth >= -Hasher::IMG_16BIT_MAX && depth <= Hasher::IMG_16BIT_MAX);
    assert(!ranges.empty());

    // Loop through histogram ranges and return quantized index
    const size_t iSize = ranges.size();
    for (size_t i = 0; i < iSize; i++) {
        if (ranges[i].start >= depth && depth < ranges[i].end) {
            return static_cast<uchar>(i);
        }
    }

    // If value is IMG_16BIT_MAX it belongs to last bin
    return static_cast<uchar>(iSize - 1);
}

uchar Processing::quantizeSurfaceNormal(const cv::Vec3f &normal) {
    // Normal z coordinate should not be < 0
    assert(normal[2] >= 0);

    // In our case z is always positive, that's why we're using
    // 8 octants in top half of sphere only to quantize into 8 bins
    static cv::Vec3f octantNormals[8] = {
            cv::Vec3f(0.707107f, 0.0f, 0.707107f), // 0. octant
            cv::Vec3f(0.57735f, 0.57735f, 0.707107f), // 1. octant
            cv::Vec3f(0.0f, 0.707107f, 0.707107f), // 2. octant
            cv::Vec3f(-0.57735f, 0.57735f, 0.707107f), // 3. octant
            cv::Vec3f(-0.707107f, 0.0f, 0.707107f), // 4. octant
            cv::Vec3f(-0.57735f, -0.57735f, 0.707107f), // 5. octant
            cv::Vec3f(0.0f, -0.707107f, 0.707107f), // 6. octant
            cv::Vec3f(0.57735f, -0.57735f, 0.707107f), // 7. octant
    };

    uchar minIndex = 9;
    float maxDot = 0, dot = 0;
    for (uchar i = 0; i < 8; i++) {
        // By doing dot product between octant octantNormals and calculated normal
        // we can find maximum -> index of octant where the vector belongs to
        dot = normal.dot(octantNormals[i]);

        if (dot > maxDot) {
            maxDot = dot;
            minIndex = i;
        }
    }

    // Index should in interval <0,7>
    assert(minIndex >= 0 && minIndex < 8);

    return minIndex;
}

uchar Processing::quantizeOrientationGradient(float deg) {
    // Checks
    assert(deg >= 0);
    assert(deg <= 360);

    // We only work in first 2 quadrants (PI)
    int degPI = static_cast<int>(deg) % 180;

    // Quantize
    if (degPI >= 0 && degPI < 36) {
        return 0;
    } else if (degPI >= 36 && degPI < 72) {
        return 1;
    } else if (degPI >= 72 && degPI < 108) {
        return 2;
    } else if (degPI >= 108 && degPI < 144) {
        return 3;
    } else {
        return 4;
    }
}

void Processing::relativeDepths(const cv::Mat &src, cv::Point c, cv::Point p1, cv::Point p2, float *depths) {
    assert(!src.empty());
    assert(src.type() == CV_16U);

    // Compute relative depths
    depths[0] = src.at<float>(p1) - src.at<float>(c);
    depths[1] = src.at<float>(p2) - src.at<float>(c);
}
