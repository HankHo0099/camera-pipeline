# C++ Multithreaded Image Processing Pipeline

## Overview
A C++17 and OpenCV project that compares sequential and parallel image processing.
Custom pixel operations and standard C++ synchronization demonstrate practical
concurrency, throughput measurement, and worker scaling on an offline video.
The benchmark processes every frame without input throttling or frame dropping;
it prints statistics and discards processed images rather than displaying video.

## Architecture
`VideoCapture (Producer) -> Bounded Queue (16 frames) -> Worker Threads`

- **Producer-Consumer:** the calling thread reads video; workers process frames.
- **Bounded Queue:** a full queue blocks the producer, providing backpressure.
- **Worker Threads:** 1, 2, 4, or 8 workers process independent frames outside the lock.
- **Mutex:** protects queue operations and the end-of-input flag.
- **Condition Variable:** workers wait for frames; the producer waits for space.
- At EOF, workers drain the queue and are joined before statistics are combined.

## Image Processing
Implemented manually in [image_processing.cpp](src/image_processing.cpp).
OpenCV provides image storage, video decoding, and demo PNG encoding.

| Function | Purpose |
|---|---|
| `grayscale` | Weighted BGR-to-grayscale conversion |
| `gaussianBlur` | 3x3 Gaussian smoothing |
| `sobelEdge` | Sobel gradient magnitude, capped at 255 |
| `computeHistogram` | 256-bin intensity histogram |
| `histogramEqualization` | CDF-based contrast enhancement |

Processing order: Grayscale -> Gaussian Blur -> Sobel -> Histogram Equalization.
`computeHistogram` is called internally by `histogramEqualization`.

## Benchmark Results
Existing 10-run measurements: Windows x64 Release, Intel Core i7-10700F
(8 cores / 16 threads), 1920x1080 video, 2,207 frames, queue capacity 16.

| Mode | Median FPS | Median Speedup |
|---|---:|---:|
| Single-thread | 20.025 | 1.000x |
| 1 Worker | 26.123 | 1.305x |
| 2 Workers | 49.883 | 2.491x |
| 4 Workers | 86.773 | 4.327x |
| 8 Workers | 92.620 | 4.584x |

Speedup is the median of per-round baseline-time / mode-time ratios.
Every processing run completed all 2,207 frames. Decode-only median: 103.709 FPS.
Going from 4 to 8 workers improved median FPS by 6.74%, with greater variation.
Results show diminishing returns; they do not identify a definitive bottleneck.
Mode order was fixed; cache state, temperature, and background load were uncontrolled.
See the [complete 10-run report](BENCHMARK_10RUNS.md) for raw data and statistics.

## Build & Run
Requires CMake 3.16+, a C++17 compiler, and OpenCV (`core`, `videoio`, `imgcodecs`).
Verified environment: Windows x64, Visual Studio 2019, and vcpkg `x64-windows`.
From the repository root, configure the vcpkg toolchain (adjust its local path):

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=D:/Tools/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\benchmark_pipeline.exe data/test.mp4
.\build\Release\image_processing_demo.exe
```

Supply a local video at `data/test.mp4`; media files are excluded from Git.
The benchmark accepts an optional video path (default: `data/test.mp4`).
Each invocation runs Decode-only, Single-thread, and 1/2/4/8 Workers once.
The demo reads the first frame of `data/test.mp4` and exports five PNGs to `results/`.
Build output, input media, and generated images remain local and are Git-ignored.
