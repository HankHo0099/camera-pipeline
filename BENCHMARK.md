# Pipeline benchmark

The main executable is an offline, unrestricted-input video benchmark in
`src/benchmark_pipeline.cpp`. See [README](README.md) for the project overview and
[the 10-run report](BENCHMARK_10RUNS.md) for the primary performance results.

## Legacy Experiment boundary

`src/pipeline.cpp` is a separate **Legacy Experiment** with a configurable input
FPS limit, drop policy, and simulated consumer delay. `src/thread_demo.cpp` is a
synthetic queue/sleep experiment and is not a CMake target. Neither supplies the
current benchmark measurements. `benchmark_single` uses OpenCV grayscale and a
frame clone, so its workload differs from the full custom pipeline.

## Build and execution

Build and run from the project root (Windows x64, Release):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\benchmark_pipeline.exe data/test.mp4
```

The optional first argument selects the video; the default is `data/test.mp4`.
Existing `pipeline` and `benchmark_single` executables are unchanged.

Each invocation first runs `runDecodeOnly()`, which only reads the entire video
with `capture.read(frame)` and reports successful reads, elapsed time, and FPS.
Its Processed and Speedup columns are N/A. The executable then runs the sequential
baseline and 1, 2, 4, and 8 workers, in that fixed order.

`processFrame()` calls the existing implementations in this order:
`grayscale` -> `gaussianBlur` -> `sobelEdge` -> `histogramEqualization`.
Equalization calls `computeHistogram` internally. Each frame runs all stages;
the final image is discarded only after equalization returns.

The baseline reads and processes frames sequentially on the calling thread.
Each multithreaded run uses the calling thread as producer and starts 1, 2,
4, or 8 workers. The producer alone owns VideoCapture. Each read uses a fresh
Mat, whose ownership is moved into a queue of capacity 16. A full queue blocks
the producer on `notFull`; an empty queue blocks workers on `notEmpty`.
The mutex protects queue operations and the EOF flag. A pop removes exactly
one frame and processing runs outside the lock with local image buffers.

At EOF, the producer sets `finished` under the mutex and wakes all workers.
Workers exit only when the queue is empty, so queued frames are processed
before shutdown. The producer joins every worker before reading their counters.
There is no frame dropping, input rate limit, or simulated workload.

Timing starts before opening the video and ends after processing (including
worker startup, queue waits, and joins for multithreaded runs). Throughput is
processed frames / elapsed seconds. Speedup is baseline seconds / run seconds.
Each configuration runs once. Processing configurations must match the baseline
frame count; a mismatch or empty baseline returns a nonzero exit code. Decode-only
reports its read count separately; the program does not compare it to the baseline.

OpenCV processing threads are set to one for all runs. Video decoder backends
may still use their own threads, so the baseline describes application-level
processing. Sequential single runs are affected by file caching, CPU frequency,
thermal conditions, and background load; they are preliminary measurements.

Suggested reading order in `src/benchmark_pipeline.cpp`: `processFrame`,
`runSingle`, the worker lambda in `runWorkers`, producer queue wait/push,
EOF notification and joins, then `main` and its frame count checks.

The only algorithm fix removes the redundant, unclamped LUT construction in
`histogramEqualization` and copies constant images unchanged to avoid division
by zero. Other processing algorithms are preserved.

## Initial results

Historical single-run measurements, retained for reference. Their irregular
scaling is not used for the README conclusions; use the later
[10-run report](BENCHMARK_10RUNS.md) as the primary data source.

2026-11-08, Windows x64 Release, `data/test.mp4`, one run per mode:

| Mode | Workers | Total frames | Processed frames | Elapsed (s) | FPS | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Single-thread | 0 | 2207 | 2207 | 206.046 | 10.711 | 1.000 |
| Multithreading | 1 | 2207 | 2207 | 114.314 | 19.306 | 1.802 |
| Multithreading | 2 | 2207 | 2207 | 132.511 | 16.655 | 1.555 |
| Multithreading | 4 | 2207 | 2207 | 114.885 | 19.210 | 1.793 |
| Multithreading | 8 | 2207 | 2207 | 25.156 | 87.731 | 8.191 |

All runs finished normally with matching counts (exit code 0). The full Release
build succeeded; at the time, non-ASCII comments in `image_processing.cpp` produced
MSVC C4819 code-page warnings. Those comments were later replaced with English
documentation without changing the algorithms.

Eight workers were fastest in this run. The irregular scaling (two workers slower
than one, and a large jump from four to eight) means these results should not be
treated as stable CPU scaling or attributed to a specific bottleneck without
further measurements. Cache effects and changing machine load were not controlled.

## Documentation closeout validation

The Windows x64 Release configuration and full build succeeded after the
documentation/demo changes, with no warnings in that build. The rebuilt demo
exited successfully and exported the first frame through all custom stages to
`results/`. All five PNGs decoded correctly at 1920x1080: the original is color
and the four processed images are single-channel. All have nonconstant pixel
values spanning 0-255. Visual inspection found the expected grayscale, smoothing,
edge, and equalized-edge content, without obvious uninitialized regions. The
source frame is a mostly black title card; Gaussian/Sobel zero borders are
intentional. This is a first-frame sanity check, not a quality assessment.

One complete invocation of the rebuilt `benchmark_pipeline` also exited with
code 0. Decode-only read 2,207 frames; sequential and all four worker modes each
read and processed 2,207 frames. No deadlock or count mismatch was observed.

This functional check was slower than the historical 10-run measurements:
Decode-only measured 28.747 FPS; Single-thread 9.774; 1/2/4/8 workers
17.526/20.805/20.411/17.827 FPS. The cause was not measured. These single-run
results do not replace the historical statistics in `BENCHMARK_10RUNS.md` or
the README. The processing algorithms and benchmark execution logic were
verified unchanged apart from comments; the 10-run report was preserved byte
for byte.
