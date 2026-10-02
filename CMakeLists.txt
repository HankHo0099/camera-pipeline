cmake_minimum_required(VERSION 3.16)

project(CameraPipeline)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

add_executable(camera_pipeline
    src/main.cpp
)

target_link_libraries(camera_pipeline
    PRIVATE
    opencv_core
    opencv_videoio
    opencv_imgproc
)
