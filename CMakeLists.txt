cmake_minimum_required(VERSION 3.20)

project(FasterMaximumClique LANGUAGES CXX)


include(CTest)
enable_testing()

# Add LazyMC as a subdirectory; all its internal paths remain unchanged
add_subdirectory(LazyMC)