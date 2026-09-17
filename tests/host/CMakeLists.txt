# Host unit tests — no hardware; fake I2C adapter replays data-sheet frames.
add_executable(hf_sfm_host_tests sfm_driver_test.cpp)
target_link_libraries(hf_sfm_host_tests PRIVATE hf::sfm)
target_compile_options(hf_sfm_host_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME hf_sfm_host_tests COMMAND hf_sfm_host_tests)
