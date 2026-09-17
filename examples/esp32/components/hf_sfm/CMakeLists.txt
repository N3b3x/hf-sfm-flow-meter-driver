# hf_sfm — ESP-IDF component wrapper (header-only driver)
set(HF_SFM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." CACHE PATH
    "Path to hf-sfm-flow-meter-driver root")

get_filename_component(HF_SFM_ROOT "${HF_SFM_ROOT}" ABSOLUTE)
if(NOT EXISTS "${HF_SFM_ROOT}/cmake/hf_sfm_build_settings.cmake")
    message(FATAL_ERROR
        "Cannot find HF-SFM driver root at ${HF_SFM_ROOT}")
endif()

include("${HF_SFM_ROOT}/cmake/hf_sfm_build_settings.cmake")

idf_component_register(
    INCLUDE_DIRS ${HF_SFM_PUBLIC_INCLUDE_DIRS}
    REQUIRES ${HF_SFM_IDF_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_17)
