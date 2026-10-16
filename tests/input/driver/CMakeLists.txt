cmake_minimum_required(VERSION 3.25)
project(pinyon_wheelkit_driver_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(WHEELKIT_SDK_SOURCE "" CACHE PATH "Isolated reviewed SDK source (e1a2531c)")
set(DBCE_TOOLKIT_SOURCE "" CACHE PATH "Inspected local toolkit source")
if(NOT EXISTS "${WHEELKIT_SDK_SOURCE}/tests/unit/input/remap_seam/CMakeLists.txt" OR
   NOT EXISTS "${DBCE_TOOLKIT_SOURCE}/native/shim/shim/shim.hpp")
  message(FATAL_ERROR "Supply reviewed SDK and toolkit source paths explicitly")
endif()
# Reuse the reviewed fixture's support imports/header setup without rebuilding or
# rerunning its completed test target. No submodule checkout/gitlink is modified.
add_subdirectory("${WHEELKIT_SDK_SOURCE}/tests/unit/input/remap_seam"
  "${CMAKE_CURRENT_BINARY_DIR}/sdk-seam" EXCLUDE_FROM_ALL)
find_package(Threads REQUIRED)
set(shim_root "${DBCE_TOOLKIT_SOURCE}/native/shim")
get_filename_component(app_root "../../.." ABSOLUTE)
get_target_property(seam_includes remap_seam_tests INCLUDE_DIRECTORIES)
get_target_property(seam_links remap_seam_tests LINK_LIBRARIES)
add_executable(pinyon_wheelkit_driver_tests
  ../wheelkit_driver_test.cpp
  "${app_root}/src/input/wheelkit_input_driver.cpp"
  "${app_root}/src/input/wheelkit_input_lifecycle.cpp"
  "${app_root}/src/input/wheelkit_snapshot.cpp"
  "${shim_root}/shim/core.cpp" "${shim_root}/shim/parse.cpp"
  "${WHEELKIT_SDK_SOURCE}/src/input/input_system.cpp"
  "${WHEELKIT_SDK_SOURCE}/src/input/pad_remap.cpp"
  "${WHEELKIT_SDK_SOURCE}/src/input/device_assignment.cpp"
  "${SDK_DEPENDENCY_ROOT}/thirdparty/catch2/extras/catch_amalgamated.cpp")
target_include_directories(pinyon_wheelkit_driver_tests PRIVATE
  "${app_root}/src" "${shim_root}/shim" "${shim_root}/host" "${shim_root}/tests"
  "${DBCE_TOOLKIT_SOURCE}/native/controls" "${DBCE_TOOLKIT_SOURCE}/native/session"
  ${seam_includes})
target_compile_definitions(pinyon_wheelkit_driver_tests PRIVATE SPDLOG_FMT_EXTERNAL NOMINMAX)
target_compile_options(pinyon_wheelkit_driver_tests PRIVATE /W4 /EHsc /Gy)
target_link_libraries(pinyon_wheelkit_driver_tests PRIVATE ${seam_links} Threads::Threads)
enable_testing()
add_test(NAME wheelkit_driver COMMAND pinyon_wheelkit_driver_tests)
set_tests_properties(wheelkit_driver PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${SDK_RUNTIME_ARTIFACTS}" TIMEOUT 15)
