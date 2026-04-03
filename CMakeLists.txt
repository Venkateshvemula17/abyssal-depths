cmake_minimum_required(VERSION 3.16)
project(DungeonCrawlerServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
pkg_check_modules(HIREDIS REQUIRED hiredis)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${PQXX_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    /usr/local/include
)

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(dungeon_server ${SOURCES})

target_link_libraries(dungeon_server
    ${PQXX_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    uSockets
    ssl
    crypto
    z
    pthread
)

target_compile_options(dungeon_server PRIVATE -O2 -Wall)
