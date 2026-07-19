cmake_minimum_required(VERSION 3.20)
project(landscape_audit VERSION 0.1.0 LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(landscape_audit INTERFACE)
target_include_directories(landscape_audit INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

add_executable(lsaudit examples/lsaudit.cpp)
target_link_libraries(lsaudit PRIVATE landscape_audit)
target_link_libraries(lsaudit PRIVATE Threads::Threads)
target_include_directories(lsaudit PRIVATE examples)

add_executable(landscape_audit_tests tests/test_audit.cpp)
target_link_libraries(landscape_audit_tests PRIVATE landscape_audit)
target_link_libraries(landscape_audit_tests PRIVATE Threads::Threads)
target_include_directories(landscape_audit_tests PRIVATE examples)

foreach(target lsaudit landscape_audit_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

enable_testing()
add_test(NAME landscape_audit_tests COMMAND landscape_audit_tests)
add_test(NAME neutral_trap_demo
    COMMAND lsaudit neutral-trap
        --certificate ${CMAKE_CURRENT_BINARY_DIR}/neutral-trap.json
        --graph ${CMAKE_CURRENT_BINARY_DIR}/neutral-trap.graphml)
add_test(NAME neutral_trap_replay
    COMMAND lsaudit neutral-trap
        --verify ${CMAKE_CURRENT_BINARY_DIR}/neutral-trap.json)
set_tests_properties(neutral_trap_replay PROPERTIES DEPENDS neutral_trap_demo)

install(TARGETS lsaudit RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
