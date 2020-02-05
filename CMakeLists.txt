cmake_minimum_required (VERSION 3.7)
project(pizza)

set(EXECUTABLE_OUTPUT_PATH ../)

#set(CMAKE_CXX_COMPILER /usr/bin/g++-8)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_REQUIRED_FLAGS -std=c++17)

add_executable(pizzaservice 	
	src/server.cpp 
	src/http_server.cpp
	src/http_server.hpp
	src/responses.cpp
	src/utils.hpp
	src/utils.cpp
	include/cpp-jwt/jwt/jwt.hpp # https://github.com/arun11299/cpp-jwt
	include/sqlite_modern_cpp/hdr/sqlite_modern_cpp.h
)

find_package(OpenSSL REQUIRED) # needed by jwt-cpp
find_package(Boost 1.67 REQUIRED)

include_directories(
	${OPENSSL_INCLUDE_DIR}
	${BOOST_INCLUDE_DIRS}
	../include/sqlite_modern_cpp/hdr/sqlite_modern_cpp.h
)

target_compile_options(pizzaservice PUBLIC -Wall -Wextra -g -DBOOST_SYSTEM_NO_DEPRECATED) 

target_link_libraries(pizzaservice PUBLIC
	${OPENSSL_LIBRARIES}
	-lpthread # boost::beast
	#-lstdc++fs # std::filesystem
	-lboost_system # neccessary on ubuntu 16 and 18
	#boost
	-lsqlite3
)
