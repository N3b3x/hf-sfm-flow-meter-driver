---
layout: default
title: "CMake integration"
nav_order: 5
parent: "Documentation"
permalink: /docs/cmake-integration/
---

# CMake integration

## Standalone consumer

```cmake
add_subdirectory(path/to/hf-sfm-flow-meter-driver)
target_link_libraries(app PRIVATE hf::sfm)
```

## Settings file only

`cmake/hf_sfm_build_settings.cmake` is `include_guard`ed and sets:

| Variable | Purpose |
|----------|---------|
| `HF_SFM_TARGET_NAME` | `hf_sfm` |
| `HF_SFM_VERSION*` | semantic version → generated `sfm_version.h` |
| `HF_SFM_PUBLIC_INCLUDE_DIRS` | `inc/` + generated header dir |
| `HF_SFM_SOURCE_FILES` | empty (header-only) |
| `HF_SFM_IDF_REQUIRES` | `driver` (ESP-IDF component wrapper) |

## Options

| Option | Default | Effect |
|--------|---------|--------|
| `HF_SFM_ENABLE_WARNINGS` | OFF | `-Wall -Wextra -Wpedantic` on consumers |
| `HF_SFM_BUILD_TESTS` | OFF | builds `tests/host` and registers with CTest |

## ESP-IDF

`examples/esp32/components/hf_sfm/CMakeLists.txt` wraps the settings file as
an IDF component; add the component directory to `EXTRA_COMPONENT_DIRS`.

**Next:** [API reference →](api_reference.md)
