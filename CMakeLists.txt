cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(APP_VERSION "0.0.1" CACHE STRING "Application Version" FORCE)
project(json VERSION ${APP_VERSION})
set(SOURCE_DIR "src")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(${PROJECT_NAME} ${SOURCES})
add_definitions(-lm)
