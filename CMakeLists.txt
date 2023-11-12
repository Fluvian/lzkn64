# CMakeLists.txt for lzkn64

cmake_minimum_required(VERSION 3.10)

project(lzkn64)

set(SOURCES
    lzkn64.c
)

add_executable(${PROJECT_NAME} ${SOURCES})