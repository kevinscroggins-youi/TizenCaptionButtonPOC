# =============================================================================
# © You i Labs Inc. 2000-2020. All rights reserved.
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# The name of the project. When building for Android, this is passed from Android Studio.
if(NOT YI_PROJECT_NAME)
    set(YI_PROJECT_NAME "TizenCaptionButton")
endif()

# This is used solely for projects that are built within the You.i Engine repository and can be removed for commercial projects.
# NOTE: This is only used by Android Gradle builds, as that build system passes the YouiEngine_DIR down to CMake for each architecture being built.
include(${YouiEngine_DIR}/cmake/Hunter/YiInitializeHunter.cmake OPTIONAL)

# project() loads the toolchain file, which sets YouiEngine_DIR and YI_PLATFORM, compiler settings, etc.
project(${YI_PROJECT_NAME} LANGUAGES C CXX)

# For yi_print_ functions
include(${YouiEngine_DIR}/cmake/Modules/YiUtility.cmake)

# Pre-defined options

# The package name used for the project.
if(NOT YI_PACKAGE_NAME)
    set(YI_PACKAGE_NAME "tv.youi.tizencaptionbutton")
endif()

# The name of the application displayed on the device.
if(NOT YI_DISPLAY_NAME)
    set(YI_DISPLAY_NAME "Tizen Caption Button")
endif()

# The url scheme used to launch the app through deep linking on various platforms.
if(YI_BUNDLE_URL_SCHEME)
    set(IOS_BUNDLE_URL_SCHEME "<string>${YI_BUNDLE_URL_SCHEME}</string>")
    set(TVOS_BUNDLE_URL_SCHEME ${IOS_BUNDLE_URL_SCHEME})
    set(UWP_BUNDLE_URL_SCHEME "<Extensions>\n\t\t\t\t<uap:Extension Category=\"windows.protocol\">\n\t\t\t\t\t<uap:Protocol Name=\"${YI_BUNDLE_URL_SCHEME}\" />\n\t\t\t\t</uap:Extension>\n\t\t\t</Extensions>")
endif()

set(YI_VERSION_NUMBER 1.0.0.0 CACHE STRING "Build version of the application. This is the version that will be displayed to the end user in the store.")
set(YI_BUILD_NUMBER "ENG-unversioned" CACHE STRING "Release version number that needs to be incremented for each store submission. For Apple platforms, this is the CFBundleVersion. For Android, this is the Version Code.")
set(YI_YOUI_ENGINE_VERSION 6.2.0 CACHE STRING "Version required for the You.i Engine.")
set(YI_EXCLUDED_ASSET_FILE_EXTENSIONS ".log,.aep" CACHE STRING "Comma-delimited list of file extensions whose files should be omitted during asset copying.")
set(YI_ENABLE_PLAYREADY_FOR_XBOX NO CACHE BOOL "Specifies that the application requires playback of PlayReady content. Off by default as the application must get approval through Microsoft to release an app with this configuration." FORCE)

yi_print_app_names(YI_PROJECT_NAME YI_PACKAGE_NAME YI_DISPLAY_NAME)
yi_print_vars(YI_TREAT_WARNINGS_AS_ERRORS  YI_VERSION_NUMBER YI_YOUI_ENGINE_VERSION)

set(_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/Staging")
set(_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# The ordering of the paths within the CMAKE_MODULE_PATH variable is imporant, since having the project's 'cmake'
# path first will tell CMake to first look under that folder when the short form 'include(Modules/<module>)' command is used
# instead of the long form 'include(<path>/Modules/<filename>.cmake)'.
# If CMake finds a module under the project directory structure, it will load that file and stop looking in the CMAKE_MODULE_PATH.
# This allows projects to customize modules as they need to, without requiring changes at the engine level.
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${YouiEngine_DIR}/cmake)

if((NOT MSVC_IDE AND NOT XCODE) AND NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "CMAKE_BUILD_TYPE needs to be set for single configuration generators. Default builds are not allowed.")
endif()

if(NOT DEFINED YI_PLATFORM)
    message(FATAL_ERROR "The YI_PLATFORM variable was not given to the generator.")
endif()

include(Platform/Yi${YI_PLATFORM_CAPITALIZED} OPTIONAL RESULT_VARIABLE HAVE_YIPLATFORM_INCLUDE)
if (NOT HAVE_YIPLATFORM_INCLUDE)
    message( FATAL_ERROR "Unsupported platform specified: " ${YI_PLATFORM_CAPITALIZED})
endif()

# For ccache, to optimize builds on UNIX system generators.
include(Modules/YiConfigureCCache)
yi_setup_ccache()

# By having the list of source files for the project outside of the CMakeLists.txt file a project can
# use tools to auto-generate the contents of that file without having a lot of commits on the CMakeLists.txt
# file. This also makes the CMakeLists.txt and SourceList.cmake files more portable to move between projects with
# minimal effort.
include("${CMAKE_CURRENT_SOURCE_DIR}/SourceList.cmake")

if(ANDROID AND IS_DIRECTORY ${YouiEngine_DIR}/core)
    add_subdirectory(${YouiEngine_DIR}/core ${CMAKE_CURRENT_BINARY_DIR}/core)

else()
    # Locating the You.i Engine is now done via the 'find_package' command. The call to 'find_package' passing the
    # 'YouiEngine_DIR' path to the HINTS argument is made. This tells CMake to look within the 'YouiEngine_DIR' path for
    # the You.i Engine. If found, the 'find_package' command will give back a 'youiengine' target that the project can
    # link against. This target contains all of the third party libraries and their include directories, as well as the
    # engine library, its include directories and any compiler definitions or options.
    if(NOT YouiEngine_FOUND)
        find_package(YouiEngine ${YI_YOUI_ENGINE_VERSION}
            HINTS
                ${YouiEngine_DIR}
            REQUIRED
        )
    endif()
endif()

include(Modules/YiPlatformSourceFiles)

set(PLATFORM_FILES
    ${PLATFORM_SOURCE_FILES_${YI_PLATFORM_UPPER}}
    ${PLATFORM_SOURCE_FILES_${YI_PLATFORM_UPPER}_${YI_RENDER_TYPE}}
    ${PLATFORM_SOURCE_HEADERS_${YI_PLATFORM_UPPER}}
    ${PLATFORM_SOURCE_HEADERS_${YI_PLATFORM_UPPER}_${YI_RENDER_TYPE}}
)

if(EXCLUDED_PLATFORM_FILES)
    list(REMOVE_ITEM PLATFORM_FILES ${EXCLUDED_PLATFORM_FILES})
endif()

set(YI_PROJECT_CODE_FILES
    ${YI_PROJECT_SOURCE}
    ${YI_PROJECT_HEADERS}
    ${YI_APP_SOURCE_FILES}
    ${YI_APP_SOURCE_HEADERS}
    ${PLATFORM_FILES}
)

# Some platforms, such as IOS and TVOS have resources that are used for setting up the application
# icons. By making this include OPTIONAL, if no YiPlatformResources.cmake file exists, the build
# will continue without failing.
include(Modules/${YI_PLATFORM_LOWER}/YiPlatformResources OPTIONAL)

unset(CMAKE_DEBUG_POSTFIX)

# The yi_add_executable function can be swapped with yi_add_library, if your project needs to
# output a library instead of an application executable.
include(Modules/YiAddExecutable)
yi_add_executable(TARGET ${PROJECT_NAME}
    SRC_FILES ${YI_PROJECT_CODE_FILES}
    EXTRA_FILES ${YI_PLATFORM_RESOURCES_${YI_PLATFORM_UPPER}}
)

# NOTE: This is only valid for Visual Studio-based generators
set_directory_properties(PROPERTIES
    VS_STARTUP_PROJECT ${PROJECT_NAME}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${_SRC_DIR}
    PRIVATE ${YouiEngine_DIR}/templates/mains/src
    PRIVATE ${YI_PLATFORM_INCLUDE_DIRECTORIES_${YI_PLATFORM_UPPER}}
)

# Linking with the You.i Engine library is now as simple as adding the 'youi::engine' target
# to the 'target_link_libraries' command. Thanks to CMake's feature called Transitive Dependencies,
# by linking with the 'youiengine' library, the project will inherit all of the compiler settings,
# include directories and libraries that the 'youi::engine' target makes public.
#
# This makes linking to the You.i Engine and all of the third party libraries much easier, as there
# is only one target that has to be linked against.
target_link_libraries(${PROJECT_NAME}
    PRIVATE youi::engine
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RESOURCE "${YI_PLATFORM_RESOURCES_${YI_PLATFORM_UPPER}}"
)

source_group("Header Files" FILES ${YI_PROJECT_HEADERS})
source_group("Source Files" FILES ${YI_PROJECT_SOURCE})
source_group("Header Files\\App" FILES
    ${YI_APP_SOURCE_HEADERS}
    ${PLATFORM_SOURCE_HEADERS_${YI_PLATFORM_UPPER}}
    ${PLATFORM_SOURCE_HEADERS_${YI_PLATFORM_UPPER}_${YI_RENDER_TYPE}}
)
source_group("Source Files\\App" FILES
    ${YI_APP_SOURCE_FILES}
    ${PLATFORM_SOURCE_FILES_${YI_PLATFORM_UPPER}}
    ${PLATFORM_SOURCE_FILES_${YI_PLATFORM_UPPER}_${YI_RENDER_TYPE}}
)
source_group("Resources" FILES ${YI_PLATFORM_RESOURCES_${YI_PLATFORM_UPPER}})

# =============================================================================
# === START OF OPTIONS, COMPILER, ASSETS SETUP ================================
# =============================================================================

# Instrumented builds can help with determining code coverage.
# By using this module, the project can set up Debug builds to be able to output coverage data.
include(Modules/YiConfigureInstrumentedBuild)
yi_configure_instrumented_build(TARGET ${PROJECT_NAME})

include(Modules/YiConfigureLogging)
yi_configure_logging(TARGET ${PROJECT_NAME})

include(Modules/YiConfigureWarningsAsErrors)
yi_configure_warnings_as_errors(TARGET ${PROJECT_NAME})

include(Modules/YiConfigureCompileOptions)
yi_configure_compile_options(PROJECT_TARGET ${PROJECT_NAME})

# The configuration method is made available for a platform to set up any specific
# functionality that requires use of the target variables, such as PROJECT_NAME, etc.
if(COMMAND yi_configure_platform)
    yi_configure_platform(PROJECT_TARGET ${PROJECT_NAME})
endif()

# The ConfigureStaticAnalysis module runs clang-analyzer checks on supported platforms
include(Modules/YiConfigureStaticAnalysis)
yi_setup_static_analysis(PROJECT_TARGET ${PROJECT_NAME})

# Copy assets to the correct build directory
include(Modules/YiConfigureAssetCopying)
yi_configure_asset_copying(PROJECT_TARGET ${PROJECT_NAME}
    OUTPUT_DIRECTORY ${YI_BASE_OUTPUT_DIRECTORY}
    EXCLUDED_EXTENSIONS ${YI_EXCLUDED_ASSET_FILE_EXTENSIONS}
)

# Because not every platform requires custom packaging functionality, which is creating a
# application bundle suitable for the target platform. Having a cmake 'Package' target
# is optional only if the platform's CMake file implements the yi_configure_packaging_for_platform function. 
include(Modules/${YI_PLATFORM_LOWER}/YiConfigurePackagingForPlatform OPTIONAL)
if (COMMAND yi_configure_packaging_for_platform)
    include(Modules/YiGenerateUniqueTargetName)
    yi_generate_unique_target_name(TARGET Package PREFIX "${PROJECT_NAME}_" RESULT_VARIABLE _PACKAGE_TARGET)

    add_custom_target(${_PACKAGE_TARGET} ${YI_PACKAGE_IN_ALL})
    yi_configure_packaging_for_platform(PROJECT_TARGET ${PROJECT_NAME}
        PACKAGE_TARGET ${_PACKAGE_TARGET}
    )
endif()

# This piece is used to help clean up the IDE's list of projects. The supporting targets that
# get created by the project are loosely placed within the IDE's list, adding clutter.
# This module will take the listed TARGETS, search for them (and versions of them that contain the
# optional PREFIX) and place them under the FOLDER within the IDE's workspace.
#
# NOTE: This module should be executed at the bottom of the file, after all of the supporting TARGETS
# have been created. This is because the module will only work with the target if it exists.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
include(Modules/YiPutTargetsInFolder)
yi_put_targets_in_folder(TARGETS ArchiveAssets CopyAssets CleanAssets Package ProcessLocalizationData
    FOLDER "Targets"
    PREFIX "${PROJECT_NAME}_"
)
