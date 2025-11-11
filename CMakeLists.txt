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

# ---- Library (include ALL sources here) ----
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
  src/graph_processing.cpp     # ? inside the library list (fix for Greptile finding)
)

# ---- CLI ----
add_executable(lazymc_cli src/main.cpp)
target_link_libraries(lazymc_cli PRIVATE lazymc)

# ---- Tests (system GTest; no hard-coded Windows paths) ----
if(BUILD_TESTING)
  include(CTest)
  enable_testing()

  # Try config-mode first (MSYS2, vcpkg, Conan, etc.)
  find_package(GTest CONFIG QUIET)
  if(GTest_FOUND)
    add_executable(unit_tests
      tests/test_smoke.cpp
      tests/test_intersect_functions.cpp
      tests/test_bk.cpp
      tests/test_graph_processing.cpp
    )
    target_link_libraries(unit_tests PRIVATE
      lazymc
      GTest::gtest GTest::gtest_main
    )
    add_test(NAME unit_tests COMMAND unit_tests)
  else()
    message(WARNING "GTest not found via CONFIG. Provide CMAKE_PREFIX_PATH or install system GTest. Tests disabled.")
  endif()
endif()
