cmake_minimum_required(VERSION 3.16)

project(CameraPipeline)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(camera_pipeline
    src/main.cpp
)
target_include_directories(camera_pipeline PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(camera_pipeline
    PRIVATE
    opencv_core
    opencv_videoio
    opencv_imgproc
)


add_executable(pipeline src/pipeline.cpp)

target_include_directories(pipeline PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(pipeline PRIVATE
    opencv_core
    opencv_videoio
    opencv_imgproc
)