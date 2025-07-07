# CMakeList.txt : CMake project for mock_redis, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.10...3.26)
# Create the library first
add_library(mock_redis STATIC
    mock_redis.cpp
    mock_redis_misc.cpp
    mock_redis_hash.cpp
    mock_redis_string.cpp
    mock_redis_set.cpp
    mock_redis_list.cpp
    mock_redis_reply.cpp
    
)


 enable_testing()

find_package(GTest CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)

add_test(AllTestsInMain main)

set_property(TARGET mock_redis PROPERTY CXX_STANDARD 20)
set_property(TARGET mock_redis PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(mock_redis PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)


# Add your test1 executable from test1.cpp
add_executable(test01 test01.cpp)
set_property(TARGET test01 PROPERTY CXX_STANDARD 20)
set_property(TARGET test01 PROPERTY CXX_STANDARD_REQUIRED ON)


# Link test1 executable to mock_redis library if needed
target_link_libraries(test01 PRIVATE hiredis::hiredis mock_redis PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
