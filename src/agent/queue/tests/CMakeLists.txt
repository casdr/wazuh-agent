find_package(GTest REQUIRED)

# Queue tests
add_executable(test_queue queue_test.cpp)

target_link_libraries(test_queue PUBLIC
    queue
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main)


# SQLiteStorage tests
add_executable(test_sqlitestorage sqlitestorage_test.cpp)

target_link_libraries(test_sqlitestorage
queue
    SQLiteCpp
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main)
