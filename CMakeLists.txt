
cmake_minimum_required(VERSION 3.10)
project(Disciplined)

find_package(Curses REQUIRED)
find_package(CURL REQUIRED)


file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/sources/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sources/*.h
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/sources)

add_executable(Disciplined ${SOURCE_FILES})

target_link_libraries(Disciplined PRIVATE CURL::libcurl)
target_include_directories(Disciplined PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(Disciplined PRIVATE ${CURSES_LIBRARIES} CURL::libcurl)