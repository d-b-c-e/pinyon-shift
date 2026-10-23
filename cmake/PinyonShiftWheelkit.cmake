# Deliberately absent from ordinary builds. Private builds use the tracked pin.
option(PINYON_SHIFT_PRIVATE_WHEELKIT "Build the private default-off FH1 input adapter" OFF)
set(PINYON_SHIFT_WHEELKIT_SOURCE "${PROJECT_SOURCE_DIR}/thirdparty/dbce-shim")
if(PINYON_SHIFT_PRIVATE_WHEELKIT)
  if(NOT WIN32 OR NOT REXSDK_DIR OR
     NOT EXISTS "${PINYON_SHIFT_WHEELKIT_SOURCE}/native/shim/shim/dinput_provider.cpp")
    message(FATAL_ERROR "Private Wheelkit requires Windows, an SDK and the pinned shim source")
  endif()
  if(TARGET pinyon_shift)
    set(_wk "${PINYON_SHIFT_WHEELKIT_SOURCE}/native")
    target_sources(pinyon_shift PRIVATE
      src/input/wheelkit_app_input.cpp src/input/wheelkit_native_input.cpp
      src/input/wheelkit_device_assignment.cpp src/input/wheelkit_input_driver.cpp
      src/input/wheelkit_input_lifecycle.cpp src/input/wheelkit_snapshot.cpp
      "${_wk}/shim/shim/core.cpp" "${_wk}/shim/shim/parse.cpp"
      "${_wk}/shim/shim/dinput_provider.cpp")
    target_include_directories(pinyon_shift PRIVATE
      "${_wk}/shim/shim" "${_wk}/shim/host" "${_wk}/controls" "${_wk}/session")
    target_compile_definitions(pinyon_shift PRIVATE PINYON_SHIFT_PRIVATE_WHEELKIT=1 NOMINMAX)
    target_link_libraries(pinyon_shift PRIVATE dinput8 dxguid)
  endif()
endif()
