# =============================================================================
# © You i Labs Inc. 2000-2020. All rights reserved.

set(SOURCE_TIZEN-NACL
    src/app/tizen-nacl/TizenNaClMainDefault.cpp
)

set(EXCLUDED_TIZEN-NACL_SOURCE
    ${YouiEngine_DIR}/templates/mains/src/TizenNaClMainDefault.cpp
)

set(EXCLUDED_PLATFORM_SOURCE
    ${EXCLUDED_${YI_PLATFORM_UPPER}_SOURCE}
)

set(EXCLUDED_PLATFORM_HEADERS
    ${EXCLUDED_${YI_PLATFORM_UPPER}_HEADERS}
)

set(EXCLUDED_PLATFORM_FILES
    ${EXCLUDED_PLATFORM_SOURCE}
    ${EXCLUDED_PLATFORM_HEADERS}
)

set(YI_PROJECT_SOURCE
    src/TizenCaptionButtonApp.cpp
    src/TizenCaptionButtonAppFactory.cpp
    ${SOURCE_${YI_PLATFORM_UPPER}}
)

set(YI_PROJECT_HEADERS
    src/TizenCaptionButtonApp.h
    ${HEADERS_${YI_PLATFORM_UPPER}}
)
