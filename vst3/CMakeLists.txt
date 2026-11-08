cmake_minimum_required(VERSION 3.25.0)

# 8-Bus - channel strip of a 90s 8-bus console as VST3 plugin (Steinberg VST3 SDK, no JUCE)
#   Linux:   cmake -S vst3 -B ~/build/8bus -G Ninja -DCMAKE_BUILD_TYPE=Release
#   Windows: cmake -S vst3 -B ~/build/8bus-win -G Ninja -DCMAKE_BUILD_TYPE=Release
#                  -DCMAKE_TOOLCHAIN_FILE=vst3/cmake/mingw-w64.cmake        (32 bit: mingw-w64-i686.cmake)

option(R8_BUILD_PLUGIN "Build the VST3 plugin (needs the VST3 SDK)" ON)
set(VST3_SDK_ROOT "$ENV{HOME}/SDKs/vst3sdk" CACHE PATH "Steinberg VST3 SDK folder")

project(EightBus VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
# no build paths (home folder, user name) in the binaries: __FILE__ in the SDK asserts becomes relative
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-ffile-prefix-map=${VST3_SDK_ROOT}=vst3sdk -ffile-prefix-map=${CMAKE_SOURCE_DIR}=src
                        -ffile-prefix-map=${CMAKE_BINARY_DIR}=build)
    if(DEFINED ENV{HOME})
        add_compile_options(-ffile-prefix-map=$ENV{HOME}=~)
    endif()
endif()

#------------------------------------------------------------------ DSP core (no SDK)
add_library(r8_core STATIC
    src/core/circuit.cpp
    src/core/sections.cpp
    src/core/strip.cpp
    src/core/oversampler.cpp
    src/core/params.cpp
    src/core/engine.cpp
)
target_include_directories(r8_core PUBLIC src/core)
target_compile_options(r8_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-unused-parameter>)

#------------------------------------------------------------------ tests of the core
add_executable(r8_curves test/curves.cpp)
target_link_libraries(r8_curves PRIVATE r8_core)
add_executable(r8_enginetest test/enginetest.cpp)
target_link_libraries(r8_enginetest PRIVATE r8_core)
if(MINGW)
    target_link_options(r8_curves PRIVATE -static)
    target_link_options(r8_enginetest PRIVATE -static)
endif()

#------------------------------------------------------------------ VST3 plugin
if(R8_BUILD_PLUGIN)
    include(cmake/plugin.cmake)
endif()
