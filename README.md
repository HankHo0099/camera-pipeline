# C++ Multithreaded Image Processing Pipeline

## Project Overview

A C++17 and OpenCV learning project for comparing sequential and multithreaded
image processing. It explores how worker count and processing workload affect
throughput, diminishing parallel returns, and possible system bottlenecks.

The main executable, `benchmark_pipeline`, is an **offline video processing
benchmark**. It reads a video file as fast as possible, without a simulated input
FPS, sleeps, or frame dropping. It does not capture a live camera stream.
Processed images are discarded after the full pipeline completes; the benchmark
prints performance statistics without displaying or saving video.

## Pipeline Architecture

Sequential processing:

```text
VideoCapture
    |
    v
Read Frame
    |
    v
Image Processing
    |
    v
Next Frame
```

Multithreaded processing:

```text
                   VideoCapture
                  (main/producer)
                         |
                         v
                Frame Queue (16)
                         |
             +-----------+-----------+
             v           v           v
          Worker 1    Worker 2    Worker N
             |           |           |
       processFrame processFrame processFrame
             |           |           |
             +-----------+-----------+
                         v
                 Join and statistics
```

The producer alone reads frames. Workers remove different frames from a shared
queue protected by a mutex. Condition variables let workers wait for frames and
the producer wait for free space. The queue holds up to 16 frames; a full queue
blocks the producer without dropping frames.

Each worker calls `processFrame()` outside the critical section, allowing several
frames to be processed concurrently. At EOF, the producer wakes all workers;
workers drain the queue before exiting. The producer joins every worker before
combining completion counts. Completion order across workers is not guaranteed.

## Image Processing Functions

The pixel operations are implemented manually in `src/image_processing.cpp`.
OpenCV supplies image storage, video decoding, and PNG encoding for the demo.

| Function | Purpose |
|---|---|
| `grayscale` | Weighted BGR to grayscale conversion |
| `gaussianBlur` | 3×3 Gaussian convolution, normalized by 16 |
| `sobelEdge` | Horizontal/vertical Sobel gradient magnitude, capped at 255 |
| `computeHistogram` | 256-bin intensity histogram |
| `histogramEqualization` | CDF-based contrast enhancement using a lookup table |

The processing order is:

```text
BGR -> Grayscale -> Gaussian Blur -> Sobel -> Histogram Equalization
```

`computeHistogram()` is called inside `histogramEqualization()` and is not a
separate pipeline stage. Grayscale accepts `CV_8UC3`; later stages use `CV_8UC1`.
Gaussian and Sobel process interior pixels and leave a one-pixel zero border.
Equalization preserves constant images. Input and output Mats must be separate.

## Multithreading Implementation

The functions below are in `src/benchmark_pipeline.cpp`:

| Function | Responsibility |
|---|---|
| `processFrame()` | Run all four stages with local intermediate Mats. |
| `runSingle()` | Read and fully process one frame before reading the next. |
| `runWorkers()` | Read on the calling thread, queue frames for fixed workers, drain at EOF, and join. |
| `printResult()` | Print counts, elapsed time, FPS, and speedup against the sequential baseline. |
| `runDecodeOnly()` | Read the entire video without processing to measure read/decode throughput. |

`main()` runs Decode-only, then `runSingle()`, then `runWorkers()` with 1, 2, 4,
and 8 workers. It uses `printResult()` for processing results and prints the
Decode-only row separately. Processing counts must match the baseline count.

One worker still runs alongside the producer, so reading and processing can
overlap; this can outperform `runSingle()` despite using only one processing
worker. Additional workers can process independent frames simultaneously.
Returns may diminish when reading/decoding, synchronization, or shared CPU and
memory resources limit throughput. These are possible explanations, rather than
measured bottleneck diagnoses.

Timing includes video opening, reading, and complete processing, plus thread
startup, queue waits, and joins where applicable. OpenCV processing threads are
set to one; the video decoding backend may still use internal threads.

## Benchmark Results

Windows x64 Release, Intel Core i7-10700F (8 cores / 16 logical processors),
1920×1080 video, 2,207 frames. Each mode was measured 10 times with queue capacity
16 and the complete custom processing pipeline.

| Mode | Median FPS | Median Speedup |
|---|---:|---:|
| Single-thread | 20.025 | 1.000× |
| 1 Worker | 26.123 | 1.305× |
| 2 Workers | 49.883 | 2.491× |
| 4 Workers | 86.773 | 4.327× |
| 8 Workers | 92.620 | 4.584× |

Speedup is calculated against the Single-thread time within each round, then
reported as the median of those ratios. All runs read 2,207 frames, and every
processing run completed all 2,207 frames.

Decode-only median throughput was **103.709 FPS**. Increasing from 4 to 8 workers
improved median throughput by **6.74%**. Eight workers had greater variation
(88.688–109.595 FPS, sample relative standard deviation 7.89%); the other modes
were around 0.77–1.07%.

These results are consistent with diminishing returns and a possible decode or
shared-resource bottleneck, but do not establish its cause. Two 8-worker runs
exceeded their corresponding Decode-only measurements, so Decode-only is not a
fixed throughput ceiling. All rounds used the same mode order; cache state,
temperature, and background load were not controlled.

See [the complete 10-run report](BENCHMARK_10RUNS.md) for raw data and statistics
and [benchmark notes](BENCHMARK.md) for execution details. The executable currently
runs the full pipeline and has no workload-selection option.

## Build & Run

The verified setup uses Windows x64, Visual Studio 2019, CMake (minimum 3.16),
and OpenCV installed through vcpkg's `x64-windows` triplet. OpenCV must include
`core`, `videoio`, `imgproc`, and `imgcodecs`. The existing build directory already
contains the toolchain and OpenCV configuration.

From the project root, using that configured build directory:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\benchmark_pipeline.exe data/test.mp4
```

For a fresh checkout on the verified environment, configure the same toolchain
explicitly (replace the local vcpkg path if needed):

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=D:/Tools/vcpkg/scripts/buildsystems/vcpkg.cmake
```

The video path is optional and defaults to `data/test.mp4`. The `data/` directory
is Git-ignored, so supply the input video locally. One invocation runs all six
modes once, in fixed order, and may take several minutes.

To export the first frame and each processing stage without a GUI:

```powershell
.\build\Release\image_processing_demo.exe
```

The demo reads `data/test.mp4` and writes `original.png`, `grayscale.png`,
`gaussian.png`, `sobel.png`, and `equalized.png` into `results/`.

Generated images remain local in the Git-ignored `results/` directory. Equalization
is applied to the Sobel result, so the final image emphasizes edges. The demo is
a visual sanity check rather than an image-quality evaluation.

## Project Structure

| File | Role |
|---|---|
| `src/benchmark_pipeline.cpp` | Main offline benchmark: Decode-only, sequential, and worker modes |
| `src/image_processing.h`, `src/image_processing.cpp` | Custom image-processing interfaces and implementations |
| `src/image_processing_demo.cpp` | First-frame PNG export for each stage |
| `src/main.cpp` | Earlier OpenCV grayscale video-reading example (`camera_pipeline`) |
| `src/benchmark_single.cpp` | Earlier clone + OpenCV grayscale baseline; a different workload |
| `src/pipeline.cpp` | **Legacy Experiment:** configurable input rate, drop policy, and simulated consumer delay |
| `src/thread_demo.cpp` | **Legacy Experiment:** synthetic queue/drop/sleep exercise; not a CMake target |
| `CMakeLists.txt` | Executable targets, C++17, and OpenCV/thread dependencies |
| `results/` | Local first-frame validation images (Git-ignored) |
| `BENCHMARK_10RUNS.md` | Full repeated-measurement results |

The legacy experiments are preserved for learning history and are separate from
the current offline benchmark and its no-drop, unrestricted-input measurements.
