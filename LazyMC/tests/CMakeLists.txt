# CMakeLists.txt for tests directory

add_executable(lazymc_tests
  test_smoke.cpp
  test_graph_processing.cpp
  test_bk.cpp
  test_degree_heuristic.cpp
)

target_link_libraries(lazymc_tests
  PRIVATE
    lazymc
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(lazymc_tests)
