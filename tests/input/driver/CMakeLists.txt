cmake_minimum_required(VERSION 3.25)
project(pinyon_wheelkit_driver_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(WHEELKIT_SDK_SOURCE "" CACHE PATH "Explicit isolated SDK source for this candidate")
get_filename_component(app_root "../../.." ABSOLUTE)
set(DBCE_TOOLKIT_SOURCE "${app_root}/thirdparty/dbce-shim" CACHE PATH "Pinned shim source")
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
get_target_property(seam_includes remap_seam_tests INCLUDE_DIRECTORIES)
get_target_property(seam_links remap_seam_tests LINK_LIBRARIES)
add_executable(pinyon_wheelkit_driver_tests
  ../wheelkit_driver_test.cpp
  "${app_root}/src/input/wheelkit_app_input.cpp"
  "${app_root}/src/input/wheelkit_input_driver.cpp"
  "${app_root}/src/input/wheelkit_device_assignment.cpp"
  "${app_root}/src/input/wheelkit_input_lifecycle.cpp"
  "${app_root}/src/input/wheelkit_snapshot.cpp"
  "${shim_root}/shim/core.cpp" "${shim_root}/shim/parse.cpp"
  "${WHEELKIT_SDK_SOURCE}/src/input/input_system.cpp"
  "${WHEELKIT_SDK_SOURCE}/src/input/pad_remap.cpp"
  "${WHEELKIT_SDK_SOURCE}/src/input/device_assignment.cpp"
  "${WHEELKIT_SDK_SOURCE}/src/input/dualsense.cpp"
  "${SDK_DEPENDENCY_ROOT}/thirdparty/catch2/extras/catch_amalgamated.cpp")
target_include_directories(pinyon_wheelkit_driver_tests PRIVATE
  "${app_root}/src" "${shim_root}/shim" "${shim_root}/host" "${shim_root}/tests"
  "${DBCE_TOOLKIT_SOURCE}/native/controls" "${DBCE_TOOLKIT_SOURCE}/native/session"
  ${seam_includes})
target_compile_definitions(pinyon_wheelkit_driver_tests PRIVATE SPDLOG_FMT_EXTERNAL NOMINMAX)
target_compile_options(pinyon_wheelkit_driver_tests PRIVATE /W4 /EHsc /Gy)
target_link_libraries(pinyon_wheelkit_driver_tests PRIVATE ${seam_links} Threads::Threads)
enable_testing()
add_test(NAME wheelkit_driver COMMAND pinyon_wheelkit_driver_tests "[wheelkit-driver]")
add_test(NAME wheelkit_priority COMMAND pinyon_wheelkit_driver_tests "[wheelkit-priority]")
add_test(NAME wheelkit_keyboard_priority COMMAND pinyon_wheelkit_driver_tests
  "[wheelkit-keyboard-priority]")
add_test(NAME wheelkit_priority_feedback COMMAND pinyon_wheelkit_driver_tests
  "[wheelkit-priority-feedback]")
add_test(NAME wheelkit_factory COMMAND pinyon_wheelkit_driver_tests "[wheelkit-factory]")
set_tests_properties(wheelkit_driver wheelkit_priority wheelkit_keyboard_priority wheelkit_priority_feedback wheelkit_factory PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${SDK_RUNTIME_ARTIFACTS}" TIMEOUT 15)
add_test(NAME wheelkit_integration COMMAND pinyon_wheelkit_driver_tests "[wheelkit-integration]")
set_tests_properties(wheelkit_integration PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${SDK_RUNTIME_ARTIFACTS}" TIMEOUT 20)

# Compile the actual changed app hook, native provider bridge, shared provider
# and SDL backend without linking/running the game or recompiling game code.
option(WHEELKIT_COMPILE_APP_HOOKS "Compile private Windows app integration objects" OFF)
set(WHEELKIT_GENERATED_SOURCE "" CACHE PATH "Existing generated FH1 headers, read only")
if(WHEELKIT_COMPILE_APP_HOOKS)
  if(NOT EXISTS "${WHEELKIT_GENERATED_SOURCE}/pinyon_shift_init.h")
    message(FATAL_ERROR "Supply existing generated headers explicitly; no codegen is run")
  endif()
  add_library(pinyon_wheelkit_app_compile OBJECT
    "${app_root}/src/pinyon_shift_app.cpp"
    "${app_root}/src/input/wheelkit_native_input.cpp"
    "${shim_root}/shim/dinput_provider.cpp"
    "${WHEELKIT_SDK_SOURCE}/src/input/sdl/sdl_input_driver.cpp")
  get_target_property(driver_includes pinyon_wheelkit_driver_tests INCLUDE_DIRECTORIES)
  target_include_directories(pinyon_wheelkit_app_compile PRIVATE ${driver_includes}
    "${app_root}/include" "${WHEELKIT_SDK_SOURCE}" "${SDK_DEPENDENCY_ROOT}"
    "${WHEELKIT_GENERATED_SOURCE}"
    "${SDK_DEPENDENCY_ROOT}/thirdparty/imgui"
    "${SDK_DEPENDENCY_ROOT}/thirdparty/tomlplusplus/include"
    "${SDK_DEPENDENCY_ROOT}/thirdparty/renderdoc"
    "${SDK_DEPENDENCY_ROOT}/thirdparty/vulkan-headers/include"
    "${SDK_DEPENDENCY_ROOT}/thirdparty/vulkan-memory-allocator/include")
  target_compile_definitions(pinyon_wheelkit_app_compile PRIVATE
    PINYON_SHIFT_PRIVATE_WHEELKIT=1 REX_HAS_D3D12=1 REX_HAS_VULKAN=1
    REXGLUE_ENABLE_PERF_COUNTERS SPDLOG_FMT_EXTERNAL SPDLOG_COMPILED_LIB NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(pinyon_wheelkit_app_compile PRIVATE /W4 /EHsc /clang:-msse4.1)
endif()
