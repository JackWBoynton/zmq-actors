cmake_minimum_required(VERSION 3.10)
project(actor_demo)

set(CMAKE_CXX_STANDARD 23)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set(CPPZMQ_BUILD_TESTS OFF)

add_subdirectory(external/cppzmq)

# sudo apt-get install libzmq3-dev libcppzmq-dev
# find_package(cppzmq REQUIRED)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE cppzmq)

add_executable(actor actor.cpp)
target_link_libraries(actor PRIVATE cppzmq)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE cppzmq)

add_executable(benchmark_actor benchmark_actor.cpp)
target_link_libraries(benchmark_actor PRIVATE cppzmq)

add_executable(benchmark_client benchmark_client.cpp)
target_link_libraries(benchmark_client PRIVATE cppzmq)

add_executable(benchmark_server benchmark_server.cpp)
target_link_libraries(benchmark_server PRIVATE cppzmq)
