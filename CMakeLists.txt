# CMakeLists.txt for lzkn64

cmake_minimum_required(VERSION 3.10)

project(lzkn64)

set(SOURCES
    main.c
    lzkn64.c
)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})