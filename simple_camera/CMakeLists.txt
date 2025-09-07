cmake_minimum_required(VERSION 3.10)
project(simple_camera)

set(CMAKE_CXX_STANDARD 17)

# cross compile on WSL for RPi ###################
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Set the C and C++ compilers
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
###################################################

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)

add_executable(simple_camera src/main.cpp)
target_include_directories(simple_camera PRIVATE ${LIBCAMERA_INCLUDE_DIRS})
target_link_libraries(simple_camera PRIVATE ${LIBCAMERA_LIBRARIES})

# OpenCV
find_package(OpenCV REQUIRED)
target_include_directories(simple_camera PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(simple_camera PRIVATE ${OpenCV_LIBS})
