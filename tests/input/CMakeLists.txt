# Standalone source-only fixture: no SDK, generated game, devices or game files.
cmake_minimum_required(VERSION 3.25)
project(pinyon_wheelkit_snapshot_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DBCE_TOOLKIT_SOURCE "" CACHE PATH "Explicit toolkit checkout for fake-provider tests")
if(NOT EXISTS "${DBCE_TOOLKIT_SOURCE}/native/shim/shim/shim.hpp")
  message(FATAL_ERROR "Set DBCE_TOOLKIT_SOURCE to an inspected toolkit checkout")
endif()
set(shim_root "${DBCE_TOOLKIT_SOURCE}/native/shim")
find_package(Threads REQUIRED)
add_executable(pinyon_wheelkit_snapshot_tests
  wheelkit_snapshot_test.cpp
  ../../src/input/wheelkit_snapshot.cpp
  "${shim_root}/shim/core.cpp"
  "${shim_root}/shim/parse.cpp")
target_include_directories(pinyon_wheelkit_snapshot_tests PRIVATE
  ../../src "${shim_root}/shim" "${shim_root}/tests"
  "${DBCE_TOOLKIT_SOURCE}/native/controls"
  "${DBCE_TOOLKIT_SOURCE}/native/session")
target_link_libraries(pinyon_wheelkit_snapshot_tests PRIVATE Threads::Threads)
if(MSVC)
  target_compile_options(pinyon_wheelkit_snapshot_tests PRIVATE /W4 /EHsc)
else()
  target_compile_options(pinyon_wheelkit_snapshot_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME wheelkit_snapshot COMMAND pinyon_wheelkit_snapshot_tests)

if(WIN32)
  add_executable(pinyon_wheelkit_lifecycle_tests
    wheelkit_lifecycle_test.cpp
    ../../src/input/wheelkit_input_lifecycle.cpp
    ../../src/input/wheelkit_snapshot.cpp
    "${shim_root}/shim/core.cpp"
    "${shim_root}/shim/parse.cpp")
  target_include_directories(pinyon_wheelkit_lifecycle_tests PRIVATE
    ../../src "${shim_root}/shim" "${shim_root}/host"
    "${shim_root}/tests" "${DBCE_TOOLKIT_SOURCE}/native/controls"
    "${DBCE_TOOLKIT_SOURCE}/native/session")
  target_link_libraries(pinyon_wheelkit_lifecycle_tests PRIVATE Threads::Threads)
  if(MSVC)
    target_compile_options(pinyon_wheelkit_lifecycle_tests PRIVATE /W4 /EHsc)
  endif()
  add_test(NAME wheelkit_lifecycle COMMAND pinyon_wheelkit_lifecycle_tests)
endif()
