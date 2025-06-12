cmake_minimum_required(VERSION 3.16)
project(OpenCVExample)

find_package(OpenCV REQUIRED)

add_executable(main main_opencv.cpp)
target_link_libraries(main ${OpenCV_LIBS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -ffast-math -funroll-loops")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -ffast-math -funroll-loops")
endif()


