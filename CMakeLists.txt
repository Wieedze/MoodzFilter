cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)

# Modern concise way to add dependencies to your project
include (CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Change me!
# This is the internal name of the project and the name of JUCE's shared code target
# Note: This cannot have spaces (it may be 2024, but you can't have it all!)
# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs display)
set(PROJECT_NAME "MoodzFilter")

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
# You can also just have it be the same thing as PROJECT_NAME
# You may want to append the major version on the end of this (and PROJECT_NAME) ala:
#   set(PROJECT_NAME "MyPlugin_v${MAJOR_VERSION}")
# Doing so enables major versions to show up in IDEs and DAWs as separate plugins
# allowing you to change parameters and behavior without breaking existing user projects
set(PRODUCT_NAME "MoodzFilter")

# Change me! Used for the MacOS bundle name and Installers
set(COMPANY_NAME "Radikal Moodz")

# Change me! Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.radikalmoodz.moodzfilter")

# Change me! Set the plugin formats you want built
# Valid choices: AAX Unity VST VST3 AU AUv3 Standalone
set(FORMATS VST3)  # VST3 only: playbook ADR 0003

# Force JUCE to work with sccache
# instruct MSVC to embed debug info (/Z7) instead of emitting a .pdb
set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

# For simplicity, the name of the CMake project is also the name of the target
project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# Log build environment info (after project() so compiler info is available)
include(PamplejuceLog)

# ASan / TSan / RealtimeSanitizer options, must come after project()
include(Sanitizers)
if (WITH_REALTIME_SANITIZER)
    # Turns MOODZ_NONBLOCKING (source/dsp/Realtime.h) into [[clang::nonblocking]]
    add_compile_definitions(MOODZ_REALTIME_SANITIZER=1)
endif ()

# Strict C++23 everywhere, no GNU extensions (.claude/rules/cpp.md)
set(CMAKE_CXX_EXTENSIONS OFF)

# Defines the pamplejuce_* functions we call below (also fetches Catch2 at include time)
include(PamplejuceFunctions)

# JUCE is setup as a submodule in the /JUCE folder
# Locally, you must run `git submodule update --init --recursive` once
# and later `git submodule update --remote --merge` to keep it up to date
# On Github Actions, this is done as a part of actions/checkout
add_subdirectory(JUCE)

# Add any other modules you want modules here, before the juce_add_plugin call
# juce_add_module(modules/my_module)

# This adds the melatonin inspector module
add_subdirectory (modules/melatonin_inspector)

# See `docs/CMake API.md` in the JUCE repo for all config options
juce_add_plugin("${PROJECT_NAME}"
    # Icons for the standalone app
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"

    # Change me!
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"

    # On MacOS, plugin is copied to /Users/yourname/Library/Audio/Plug-Ins/
    COPY_PLUGIN_AFTER_BUILD TRUE

    # Change me!
    # A four-character plugin id
    # First character MUST be uppercase for AU formats
    PLUGIN_MANUFACTURER_CODE Rdkm

    # Change me!
    # A unique four-character plugin id
    # Note: this must have at least one upper-case character
    PLUGIN_CODE Mzf1
    FORMATS "${FORMATS}"

    # An audio effect: stereo in, stereo out, no MIDI (plugin ADR 0002)
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    # The name of your final executable
    # This is how it's listed in the DAW
    # This can be different from PROJECT_NAME and can have spaces!
    # You might want to use v${MAJOR_VERSION} here once you go to v2...
    PRODUCT_NAME "${PRODUCT_NAME}")

# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(SharedCode INTERFACE)

# Enable fast math, C++23 and a few other target defaults
pamplejuce_shared_code_defaults()


# ---------------------------------------------------------------------------
# sst-filters (Surge Synth Team, GPL v3, header only, no JUCE): the filter models of the
# dsp layer. MoodzFilter ADR 0005. No release tags upstream, so commits are pinned
# (.claude/rules/build.md). sst-basic-blocks first, so sst-filters finds its target.
# ---------------------------------------------------------------------------
CPMAddPackage(
    NAME sst-basic-blocks
    GITHUB_REPOSITORY surge-synthesizer/sst-basic-blocks
    GIT_TAG 38cf04ae6a16994cc6c82635ce7c206f2a0ca31d
    OPTIONS "SST_BASIC_BLOCKS_BUILD_TESTS OFF")
CPMAddPackage(
    NAME sst-filters
    GITHUB_REPOSITORY surge-synthesizer/sst-filters
    GIT_TAG 87a171bcba52c014850848a29174a54f0adc6606
    OPTIONS "SST_FILTERS_BUILD_TESTS OFF" "SST_FILTERS_BUILD_EXAMPLES OFF")
CPMAddPackage(
    NAME signalsmith-dsp
    GITHUB_REPOSITORY Signalsmith-Audio/dsp
    GIT_TAG v1.7.3)
# Their headers are third-party: include them as system headers so that the strict
# warnings of our dsp library (-Werror) apply to our code only
foreach (sst_target sst-basic-blocks sst-filters signalsmith-dsp)
    get_target_property(sst_includes ${sst_target} INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(${sst_target} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${sst_includes}")
endforeach ()

# ---------------------------------------------------------------------------
# dsp: the signal processing layer. A static library that never sees JUCE
# (.claude/rules/dsp.md), so it is testable and benchmarkable on its own.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE DspFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/*.h")

# Fail at configure time if a JUCE header sneaks into the dsp layer
foreach (dsp_file IN LISTS DspFiles)
    file(STRINGS "${dsp_file}" juce_includes REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]juce_")
    if (juce_includes)
        message(FATAL_ERROR "dsp layer must not include JUCE: ${dsp_file}")
    endif ()
endforeach ()

add_library(dsp STATIC ${DspFiles})
target_include_directories(dsp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_features(dsp PUBLIC cxx_std_23)
target_link_libraries(dsp PUBLIC sst-filters signalsmith-dsp)
set_target_properties(dsp PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
# No fast math here: the dsp layer relies on IEEE behaviour (NaN and infinity checks)
if (MSVC)
    target_compile_options(dsp PRIVATE /W4 /WX)
else ()
    target_compile_options(dsp PRIVATE -Wall -Wextra -Werror)
endif ()

# ---------------------------------------------------------------------------
# SharedCode: the JUCE plugin layer (source/plugin) and the editor (source/ui)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ui/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/ui/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})
target_include_directories(SharedCode INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/source")

# Adds a BinaryData target for embedding assets into the binary
pamplejuce_add_assets()

# MacOS only: Cleans up folder and target organization on Xcode.
pamplejuce_xcode_prettify()

# This is where you can set preprocessor definitions for JUCE and your plugin
target_compile_definitions(SharedCode
    INTERFACE

    # JUCE_WEB_BROWSER and JUCE_USE_CURL off by default
    JUCE_WEB_BROWSER=0  # If you set this to 1, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you set this to 1, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_VST3_CAN_REPLACE_VST2=0

    # JUCE 9 has no splash screen; licence is AGPLv3 (plugin ADR 0001)

    # lets the app known if we're Debug or Release
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"

    VERSION="${CURRENT_VERSION}"

    # JucePlugin_Name is for some reason doesn't use the nicer PRODUCT_NAME
    PRODUCT_NAME_WITHOUT_VERSION="MoodzFilter"
)

# Link to any other modules you added (with juce_add_module) here!
# Usually JUCE modules must have PRIVATE visibility
# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_add_module
# However, with Pamplejuce, you'll link modules to SharedCode with INTERFACE visibility
# This allows the JUCE plugin targets and the Tests target to link against it
target_link_libraries(SharedCode
    INTERFACE
    dsp
    Assets
    $<$<CONFIG:Debug>:melatonin_inspector>
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Link the JUCE plugin targets our SharedCode target
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# PluginTests: the plugin layer, with JUCE
pamplejuce_add_tests(TARGET PluginTests DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/plugin")

# DspTests: the dsp layer alone, without JUCE, fast
file(GLOB_RECURSE DspTestFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/dsp/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tests/dsp/*.h")
add_executable(DspTests ${DspTestFiles})
target_link_libraries(DspTests PRIVATE dsp Catch2::Catch2WithMain)
set_target_properties(DspTests PROPERTIES CXX_EXTENSIONS OFF)
catch_discover_tests(DspTests DISCOVERY_MODE PRE_TEST)

# moodz_tools_core: test signals and error metrics, without JUCE, tested by ToolsTests
file(GLOB_RECURSE ToolsCoreFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/core/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tools/core/*.h")
add_library(moodz_tools_core STATIC ${ToolsCoreFiles})
target_include_directories(moodz_tools_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tools")
target_compile_features(moodz_tools_core PUBLIC cxx_std_23)
set_target_properties(moodz_tools_core PROPERTIES CXX_EXTENSIONS OFF)
if (MSVC)
    target_compile_options(moodz_tools_core PRIVATE /W4 /WX)
else ()
    target_compile_options(moodz_tools_core PRIVATE -Wall -Wextra -Werror)
endif ()

file(GLOB_RECURSE ToolsTestFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/*.cpp")
add_executable(ToolsTests ${ToolsTestFiles})
target_link_libraries(ToolsTests PRIVATE moodz_tools_core Catch2::Catch2WithMain)
set_target_properties(ToolsTests PROPERTIES CXX_EXTENSIONS OFF)
catch_discover_tests(ToolsTests DISCOVERY_MODE PRE_TEST)

# RealtimeSanitizer canary: passes only when RTSan reports the allocation (matching its
# message, not just any crash or loader error)
if (WITH_REALTIME_SANITIZER)
    add_executable(RtsanCanary "${CMAKE_CURRENT_SOURCE_DIR}/tests/rtsan/RtsanCanary.cpp")
    target_link_libraries(RtsanCanary PRIVATE dsp)
    add_test(NAME rtsan-canary COMMAND RtsanCanary)
    set_tests_properties(rtsan-canary PROPERTIES PASS_REGULAR_EXPRESSION "RealtimeSanitizer: unsafe-library-call")
endif ()

# MoodzFilterTools: headless render, check, compare and bench through the real
# PluginProcessor (development and CI only, never shipped)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_moodz_tools_default OFF)
else ()
    set(_moodz_tools_default ON)
endif ()
option(MOODZ_BUILD_TOOLS "Build the MoodzFilterTools command line target" ${_moodz_tools_default})
if (MOODZ_BUILD_TOOLS)
    file(GLOB ToolsFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.h")
    add_executable(MoodzFilterTools ${ToolsFiles})
    # Same JUCE configuration as the plugin, like the test targets
    target_compile_definitions(MoodzFilterTools PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},COMPILE_DEFINITIONS>)
    target_link_libraries(MoodzFilterTools PRIVATE SharedCode moodz_tools_core juce::juce_audio_formats)
    set_target_properties(MoodzFilterTools PROPERTIES CXX_EXTENSIONS OFF)
endif ()

# A separate target for Benchmarks (keeps the Tests target fast)
pamplejuce_add_benchmarks()

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)
