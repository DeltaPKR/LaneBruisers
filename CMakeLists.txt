cmake_minimum_required(VERSION 3.10)
project(LaneBruisers)

set(CMAKE_CXX_STANDARD 17)

find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

add_executable(server server.cpp)
add_executable(client client.cpp)

target_link_libraries(client sfml-graphics sfml-window sfml-system)
