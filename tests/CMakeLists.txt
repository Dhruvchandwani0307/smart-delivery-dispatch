add_executable(dispatch_tests test_dispatch.cpp)
target_link_libraries(dispatch_tests PRIVATE dispatch_core Threads::Threads)
add_test(NAME dispatch_tests COMMAND dispatch_tests)
