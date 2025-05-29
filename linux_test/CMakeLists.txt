cmake_minimum_required(VERSION 3.21)
project(cubrid_odbc_test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ODBC REQUIRED)

add_executable(test_prepare test_parepare.cpp)

target_link_libraries(test_prepare ${ODBC_LIBRARIES})

target_compile_options(test_prepare PRIVATE -Wall -Wextra)

install(TARGETS test_prepare DESTINATION bin) 
