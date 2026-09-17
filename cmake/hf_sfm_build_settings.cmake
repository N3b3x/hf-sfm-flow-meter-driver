#===============================================================================
# HF-SFM Driver - Build settings
#===============================================================================

include_guard(GLOBAL)

set(HF_SFM_TARGET_NAME "hf_sfm")

set(HF_SFM_VERSION_MAJOR 1)
set(HF_SFM_VERSION_MINOR 0)
set(HF_SFM_VERSION_PATCH 0)
set(HF_SFM_VERSION
    "${HF_SFM_VERSION_MAJOR}.${HF_SFM_VERSION_MINOR}.${HF_SFM_VERSION_PATCH}")
set(HF_SFM_VERSION_STRING "${HF_SFM_VERSION}")

set(HF_SFM_VERSION_TEMPLATE
    "${CMAKE_CURRENT_LIST_DIR}/../inc/sfm_version.h.in")
set(HF_SFM_VERSION_HEADER_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/hf_sfm_generated")
set(HF_SFM_VERSION_HEADER
    "${HF_SFM_VERSION_HEADER_DIR}/sfm_version.h")

file(MAKE_DIRECTORY "${HF_SFM_VERSION_HEADER_DIR}")

if(EXISTS "${HF_SFM_VERSION_TEMPLATE}")
    configure_file(
        "${HF_SFM_VERSION_TEMPLATE}"
        "${HF_SFM_VERSION_HEADER}"
        @ONLY
    )
    message(STATUS
        "HF-SFM driver v${HF_SFM_VERSION} — generated sfm_version.h in ${HF_SFM_VERSION_HEADER_DIR}")
else()
    message(WARNING "sfm_version.h.in not found at ${HF_SFM_VERSION_TEMPLATE}")
endif()

set(HF_SFM_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${HF_SFM_VERSION_HEADER_DIR}"
)

set(HF_SFM_SOURCE_FILES "")

set(HF_SFM_IDF_REQUIRES driver)
