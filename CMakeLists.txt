cmake_minimum_required(VERSION 3.16)

project(CameraPipeline)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(image_processing_demo
    src/image_processing_demo.cpp
    src/image_processing.cpp
)

target_include_directories(image_processing_demo PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(image_processing_demo PRIVATE
    opencv_core
    opencv_videoio
    opencv_imgcodecs
)

find_package(Threads REQUIRED)
add_executable(benchmark_pipeline
    src/benchmark_pipeline.cpp
    src/image_processing.cpp
)
target_include_directories(benchmark_pipeline PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(benchmark_pipeline PRIVATE
    opencv_core
    opencv_videoio
    Threads::Threads
)
