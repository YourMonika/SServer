cmake_minimum_required(VERSION 3.12)

project(SServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREADS_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost 1.68 COMPONENTS coroutine chrono system thread REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(${PROJECT_NAME} "main.cpp" "shserver.cpp" "shserver.hpp")
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
