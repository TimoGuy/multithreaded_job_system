cmake_minimum_required(VERSION 3.28)

# Project
if(DEFINED multithreaded_job_system_GUARD)
    if(NOT multithreaded_job_system_GUARD STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
        return() # Project has been already included by someone else
    endif()
else()
    set(multithreaded_job_system_GUARD ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "multithreaded_job_system guard")
endif()
project(multithreaded_job_system
    VERSION 0.3.0
    LANGUAGES CXX
)

# C/C++ standard.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Multithreaded build.
set(CMAKE_BUILD_PARALLEL_LEVEL 16)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# Static library build.
add_library(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/job_ifc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/job_ifc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/job_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/job_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/job_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/job_source.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/job_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/job_system.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/job_timekeeper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/job_timekeeper.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/example_main.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job_source.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job_source.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example
        ${timing_reporter_macro_magic_INCLUDE_DIR}
)
