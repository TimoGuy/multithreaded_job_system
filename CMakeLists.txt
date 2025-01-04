cmake_minimum_required(VERSION 3.28)

project(multithreaded_job_system_component
    VERSION 0.3.0
    LANGUAGES CXX
)

# C/C++ standard.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Multithreaded build.
set(CMAKE_BUILD_PARALLEL_LEVEL 16)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# Static library build for `multithreaded_job_system`
add_library(multithreaded_job_system STATIC)

target_include_directories(multithreaded_job_system
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example
)

target_sources(multithreaded_job_system
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/multithreaded_job_system_public.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_ifc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_ifc.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_queue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_queue.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_source.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_source.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_system.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/job_system.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/example_main.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job_source.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job_source.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/simple_job.h
)
