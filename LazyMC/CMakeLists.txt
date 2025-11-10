cmake_minimum_required(VERSION 3.20)
project(LazyMC LANGUAGES CXX)

# ---- Options ----
option(BUILD_TESTING "Build tests" OFF)

# ---- Toolchain ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Library ----
add_library(lazymc STATIC
  src/bk.cpp
  src/corness_based_heuristic.cpp
  src/degree_based_heuristic.cpp
  src/intersect_functions.cpp
  src/k-VertexCover.cpp
  src/lazygraph.cpp
  src/lazymc.cpp
  src/NeighborSearch.cpp
  src/Systematic_search.cpp
)

# ---- CLI ----
add_executable(lazymc_cli src/main.cpp)
target_link_libraries(lazymc_cli PRIVATE lazymc)

# ---- Tests (use system-installed GTest; no downloads) ----
if(BUILD_TESTING)
  include(CTest)
  enable_testing()

  # Try to find GTest installed by MSYS2 (pacman).
  # CMAKE_PREFIX_PATH helps CMake find it under C:\msys64\mingw64
  if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")
  endif()

  find_package(GTest CONFIG QUIET)
  if(GTest_FOUND)
    add_executable(unit_tests
      tests/test_smoke.cpp
    )
    target_link_libraries(unit_tests PRIVATE
      lazymc
      GTest::gtest GTest::gtest_main
    )
    add_test(NAME unit_tests COMMAND unit_tests)
  else()
    message(WARNING "GTest not found in system. Tests are disabled.")
  endif()
endif()
target_sources(lazymc PRIVATE src/graph_processing.cpp)
