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
    src/model/a0.cpp
    src/model/a1.cpp
    src/model/a2.cpp
    src/render/a0.cpp
    src/render/a1.cpp
    src/render/a2.cpp
    src/inference/constraint.cpp
    src/inference/automaton.cpp
    src/inference/a1_constraint.cpp
    src/inference/a1_problem.cpp
    src/inference/a1_gac.cpp
    src/inference/a1_feasibility.cpp
    src/inference/a1_support_query.cpp
    src/inference/a1_supports.cpp
    src/inference/a2_problem.cpp
    src/inference/a2_solver.cpp
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

add_library(mcr_experiments src/experiments/cameras.cpp src/experiments/a0.cpp src/experiments/a1.cpp)
target_link_libraries(mcr_experiments PUBLIC mcr)
add_executable(mcr_a0_benchmark experiments/a0_benchmark.cpp)
target_link_libraries(mcr_a0_benchmark PRIVATE mcr_experiments)
target_compile_definitions(mcr_a0_benchmark PRIVATE
    MCR_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    MCR_BUILD_TYPE="$<CONFIG>")
add_executable(mcr_a1_benchmark experiments/a1_benchmark.cpp)
target_link_libraries(mcr_a1_benchmark PRIVATE mcr_experiments)
target_compile_definitions(mcr_a1_benchmark PRIVATE
    MCR_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    MCR_BUILD_TYPE="$<CONFIG>")

if(BUILD_TESTING)
    function(mcr_test name source)
        add_executable(${name} ${source})
        target_include_directories(${name} PRIVATE tests)
        target_link_libraries(${name} PRIVATE mcr_experiments)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()
    add_test(NAME a1_benchmark_smoke COMMAND mcr_a1_benchmark
        --output "${CMAKE_CURRENT_BINARY_DIR}/a1_benchmark_smoke" --repetitions 1 --limit-per-suite 2 --csv-rows-per-file 1)
    mcr_test(geometry_tests tests/geometry_tests.cpp)
    mcr_test(render_tests tests/render_tests.cpp)
    mcr_test(a1_render_tests tests/a1_render_tests.cpp)
    mcr_test(a1_geometry_tests tests/a1_geometry_tests.cpp)
    mcr_test(a2_geometry_tests tests/a2_geometry_tests.cpp)
    mcr_test(a2_inference_tests tests/a2_inference_tests.cpp)
    mcr_test(constraint_tests tests/constraint_tests.cpp)
    mcr_test(a1_constraint_tests tests/a1_constraint_tests.cpp)
    mcr_test(a1_propagation_tests tests/a1_propagation_tests.cpp)
    target_compile_definitions(a1_propagation_tests PRIVATE
        MCR_A1_PROPAGATION_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/a1_propagation.txt")
    mcr_test(a1_feasibility_tests tests/a1_feasibility_tests.cpp)
    target_compile_definitions(a1_feasibility_tests PRIVATE
        MCR_A1_PROPAGATION_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/a1_propagation.txt")
    mcr_test(a1_support_query_tests tests/a1_support_query_tests.cpp)
    target_compile_definitions(a1_support_query_tests PRIVATE
        MCR_A1_PROPAGATION_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/a1_propagation.txt")
    mcr_test(a1_projection_tests tests/a1_projection_tests.cpp)
    target_compile_definitions(a1_projection_tests PRIVATE
        MCR_A1_PROPAGATION_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/a1_propagation.txt")
    mcr_test(inference_tests tests/inference_tests.cpp)
    mcr_test(adversarial_tests tests/adversarial_tests.cpp)
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
        target_link_libraries(a0_acceptance PRIVATE mcr_experiments)
        add_test(NAME a0_acceptance COMMAND a0_acceptance "${MCR_ORACLE_DIR}"
                 "${CMAKE_CURRENT_BINARY_DIR}/a0_acceptance.json")
        set_tests_properties(a0_acceptance PROPERTIES FIXTURES_REQUIRED python_a0)
        set(MCR_A1_ORACLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/oracle_a1" CACHE PATH "A1 oracle fixture directory")
        add_test(NAME python_a1_oracle_export COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle/export_a1.py" "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_oracle_export PROPERTIES FIXTURES_SETUP python_a1)
        add_test(NAME python_a1_render_agreement COMMAND a1_render_tests "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_render_agreement PROPERTIES FIXTURES_REQUIRED python_a1)
        add_test(NAME python_a1_constraint_agreement COMMAND a1_constraint_tests "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_constraint_agreement PROPERTIES FIXTURES_REQUIRED python_a1)
        add_test(NAME python_a1_propagation_export COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle/export_a1_propagation.py" "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_propagation_export PROPERTIES FIXTURES_SETUP python_a1_propagation)
        add_test(NAME python_a1_propagation_agreement COMMAND a1_propagation_tests "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_propagation_agreement PROPERTIES FIXTURES_REQUIRED python_a1_propagation)
        add_test(NAME python_a1_feasibility_export COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle/export_a1_feasibility.py" "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_feasibility_export PROPERTIES FIXTURES_SETUP python_a1_feasibility)
        add_test(NAME python_a1_feasibility_agreement COMMAND a1_feasibility_tests "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_feasibility_agreement PROPERTIES FIXTURES_REQUIRED python_a1_feasibility)
        add_test(NAME python_a1_support_query_export COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle/export_a1_support_query.py" "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_support_query_export PROPERTIES FIXTURES_SETUP python_a1_support_query)
        add_test(NAME python_a1_support_query_agreement COMMAND a1_support_query_tests "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_support_query_agreement PROPERTIES FIXTURES_REQUIRED python_a1_support_query)
        add_test(NAME python_a1_projection_export COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle/export_a1_projection.py" "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_projection_export PROPERTIES
            FIXTURES_REQUIRED python_a1_support_query FIXTURES_SETUP python_a1_projection)
        add_test(NAME python_a1_projection_agreement COMMAND a1_projection_tests "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_projection_agreement PROPERTIES FIXTURES_REQUIRED python_a1_projection)
        add_test(NAME python_a1_acceptance_export COMMAND "${Python3_EXECUTABLE}"
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle/export_a1_acceptance.py" "${MCR_A1_ORACLE_DIR}")
        set_tests_properties(python_a1_acceptance_export PROPERTIES FIXTURES_SETUP python_a1_acceptance)
        add_executable(a1_acceptance tests/a1_acceptance.cpp)
        target_include_directories(a1_acceptance PRIVATE tests)
        target_link_libraries(a1_acceptance PRIVATE mcr_experiments)
        add_test(NAME a1_acceptance COMMAND a1_acceptance "${MCR_A1_ORACLE_DIR}"
                 "${CMAKE_CURRENT_BINARY_DIR}/a1_acceptance.json")
        set_tests_properties(a1_acceptance PROPERTIES FIXTURES_REQUIRED python_a1_acceptance)
    endif()
endif()
