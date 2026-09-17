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
    src/experiments/cameras.cpp
    src/experiments/a0.cpp
    src/model/a0.cpp
    src/render/a0.cpp
    src/inference/constraint.cpp
    src/inference/gac.cpp
    src/inference/a0_solver.cpp)
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
        target_include_directories(${name} PRIVATE tests)
        target_link_libraries(${name} PRIVATE mcr)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()
    mcr_test(geometry_tests tests/geometry_tests.cpp)
    mcr_test(render_tests tests/render_tests.cpp)
    mcr_test(constraint_tests tests/constraint_tests.cpp)
    mcr_test(inference_tests tests/inference_tests.cpp)
    option(MCR_ENABLE_PYTHON_ORACLE "Validate C++ against the frozen Python/NumPy oracle" ON)
    if(MCR_ENABLE_PYTHON_ORACLE)
        find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
        execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import numpy"
                        RESULT_VARIABLE MCR_NUMPY_STATUS ERROR_QUIET)
        if(NOT MCR_NUMPY_STATUS EQUAL 0)
            message(FATAL_ERROR "Cross-language tests require NumPy; install it or set MCR_ENABLE_PYTHON_ORACLE=OFF")
        endif()
        set(MCR_ORACLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/oracle")
        add_test(NAME python_oracle_export COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle/export_a0.py" "${MCR_ORACLE_DIR}")
        set_tests_properties(python_oracle_export PROPERTIES FIXTURES_SETUP python_a0)
        add_test(NAME python_render_agreement COMMAND render_tests "${MCR_ORACLE_DIR}")
        set_tests_properties(python_render_agreement PROPERTIES FIXTURES_REQUIRED python_a0)
        add_executable(a0_acceptance tests/a0_acceptance.cpp)
        target_include_directories(a0_acceptance PRIVATE tests)
        target_link_libraries(a0_acceptance PRIVATE mcr)
        add_test(NAME a0_acceptance COMMAND a0_acceptance "${MCR_ORACLE_DIR}"
                 "${CMAKE_CURRENT_BINARY_DIR}/a0_acceptance.json")
        set_tests_properties(a0_acceptance PROPERTIES FIXTURES_REQUIRED python_a0)
    endif()
endif()
