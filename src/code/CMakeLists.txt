cmake_minimum_required(VERSION 3.10)

# set the project name
project(CTS)

# enable modern C++
set(CMAKE_CXX_STANDARD 17)

# add the executable
add_executable(Test.exe main_test.cpp Server.cpp Client.cpp Crypto.cpp)

# add test for the executable
enable_testing()
add_test(test1 Test.exe)
