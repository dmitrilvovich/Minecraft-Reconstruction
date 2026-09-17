cmake_minimum_required(VERSION 3.20)
project(mcr VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MCR_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
add_library(mcr
    src/math/rational.cpp
    src/geometry/aabb.cpp
    src/grid/grid.cpp
    src/grid/traversal.cpp
    src/camera/pinhole.cpp
    src/experiments/cameras.cpp)
target_include_directories(mcr PUBLIC include)
target_compile_features(mcr PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(mcr PRIVATE /W4 /permissive-)
else()
    target_compile_options(mcr PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
if(MCR_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "Use GCC or Clang for the sanitizer preset")
    endif()
    target_compile_options(mcr PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mcr PUBLIC -fsanitize=address,undefined)
endif()

if(BUILD_TESTING)
    function(mcr_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE mcr)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()
    mcr_test(geometry_tests tests/geometry_tests.cpp)
endif()
