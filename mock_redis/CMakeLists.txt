# CMakeList.txt : CMake project for mock_redis, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (mock_redis
mock_redis.cpp
hiredis.cpp)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET mock_redis PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
