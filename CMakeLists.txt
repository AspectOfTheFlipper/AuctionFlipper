cmake_minimum_required(VERSION 3.21)
project(AuctionFlipper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(PKG_CONFIG_EXECUTABLE "/usr/bin/pkg-config")
add_executable(AuctionFlipper src/main.cpp)
#Finding Libraries
set(PROJECT_INCLUDE_DIR "include")
#set(PISTACHE_INCLUDE_DIR "include/pistache")
#set(PISTACHE_LIBRARY "${PROJECT_SOURCE_DIR}/lib/libpistache.so")
#set(HEADER_SEARCH_PATHS ${PROJECT_INCLUDE_DIR} ${PISTACHE_INCLUDE_DIR})
#set(LIBRARIES_SEARCH_PATHS ${PISTACHE_LIBRARY})
find_package(nlohmann_json 3.2.0 REQUIRED)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")
#Building
include_directories(${HEADER_SEARCH_PATHS})
target_link_libraries(AuctionFlipper PRIVATE ${LIBRARIES_SEARCH_PATHS})
target_link_libraries(AuctionFlipper PRIVATE nlohmann_json::nlohmann_json)

#CurlPP reserved
# cURLpp requires libcurl to be installed
message(STATUS "Looking for curl...")
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

target_link_libraries(AuctionFlipper PRIVATE ${CURL_LIBRARIES})
target_link_libraries(AuctionFlipper PRIVATE ${CURLPP_LIBRARY})