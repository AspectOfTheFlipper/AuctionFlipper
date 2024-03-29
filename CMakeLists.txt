cmake_minimum_required(VERSION 3.17)
project(AuctionFlipper)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(PKG_CONFIG_EXECUTABLE "/usr/bin/pkg-config")
add_executable(AuctionFlipper src/main.cpp src/nbt.hpp src/zstr.hpp
        src/strict_fstream.hpp src/crow_all.h src/json.hpp)
target_include_directories(AuctionFlipper PUBLIC src/cppcodec)
#ZLIB
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif (ZLIB_FOUND)
link_directories(/usr/lib/x86_64-linux-gnu)
#Finding Libraries
#set(PROJECT_INCLUDE_DIR "include")
#set(NBT_INCLUDE_DIR "include/libnbt++")
#set(PISTACHE_LIBRARY "${PROJECT_SOURCE_DIR}/lib/libpistache.so")
#set(HEADER_SEARCH_PATHS ${PROJECT_INCLUDE_DIR} ${NBT_INCLUDE_DIR})
#set(LIBRARIES_SEARCH_PATHS ${PISTACHE_LIBRARY})
find_package(nlohmann_json 3.2.0 REQUIRED)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -m64 -std=c++20 -pthread")
#Building
#include_directories("include")
#target_link_libraries(AuctionFlipper PRIVATE ${LIBRARIES_SEARCH_PATHS})
target_link_libraries(AuctionFlipper PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(AuctionFlipper PRIVATE "${ZLIB_LIBRARIES}")
#target_link_libraries(AuctionFlipper PRIVATE "${PROJECT_SOURCE_DIR}/lib/libnbt++.a")
#CurlPP reserved
# cURLpp requires libcurl to be installed
message(STATUS "Looking for curl...")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AuctionFlipper PRIVATE "${ZLIB_LIBRARIES}")
#add_library(NBTLib STATIC "${NBT}")
find_package(CURL REQUIRED)

if (CURL_FOUND)
    message(STATUS "Found curl version: ${CURL_VERSION_STRING}")
    message(STATUS "Using curl version: ${CURL_INCLUDE_DIRS}")
    message(STATUS "Using curl libraries: ${CURL_LIBRARIES}\n")
    list(APPEND Project_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "Could not find curl.")
endif ()


message(STATUS "Looking for curlpp...")
find_package(CURLPP REQUIRED)

if (CURLPP_FOUND)
    message(STATUS "Found curlpp version: ${CURLPP_VERSION}")
    message(STATUS "Using curlpp include dir: ${CURLPP_INCLUDE_DIR}")
    message(STATUS "Using curlpp libraries: ${CURLPP_LIBRARY}\n")
    list(APPEND Project_INCLUDE_DIRS ${CURLPP_INCLUDE_DIR})
else ()
    message(FATAL_ERROR "Could not find curlpp.")
endif ()


include(FetchContent)

FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG tags/v1.0.0
        GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(simdjson)
target_link_libraries(AuctionFlipper PRIVATE ${CURL_LIBRARIES})
target_link_libraries(AuctionFlipper PRIVATE ${CURLPP_LIBRARY})
target_link_libraries(AuctionFlipper PRIVATE simdjson)
#target_link_libraries(AuctionFlipper PRIVATE NBTLib)
