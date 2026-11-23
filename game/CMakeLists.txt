# Tank: Zone of Death 1.51.3 - modern (VS 2022 / CMake) build of the 2010 sources.
# Differences from the original VS2008 project:
#   * OpenGL renderer only; the Direct3D 9 renderer needed d3dx9math from the retired DirectX SDK
#   * dxerr.h replaced by a shim (src/shim); DirectSound/DirectInput come from the Windows SDK
#   * std::tr1 replaced by std
cmake_minimum_required(VERSION 3.20)
project(tzod151 C CXX)

if(NOT MSVC)
	message(FATAL_ERROR "This port targets MSVC on Windows")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# static CRT so the exe runs on a machine without the redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
add_compile_options(/W3 /wd4996 /wd4244 /wd4267 /wd4305 /MP)

###############################################################################
# third party, as vendored by the original author

file(GLOB LUA_SOURCES src/lua/src/*.c)
list(FILTER LUA_SOURCES EXCLUDE REGEX "/(lua|luac)\.c$")
add_library(lua STATIC ${LUA_SOURCES} src/pluto/pluto.c)
target_include_directories(lua PUBLIC src/lua/src src/pluto)

file(GLOB ZLIB_SOURCES src/zlib/*.c)
list(FILTER ZLIB_SOURCES EXCLUDE REGEX "/(example|minigzip)\.c$")
add_library(zlib STATIC ${ZLIB_SOURCES})
target_include_directories(zlib PUBLIC src/zlib)

file(GLOB OGGVORBIS_SOURCES src/oggvorbis/ogg/*.c src/oggvorbis/vorbis/*.c)
# the encoder-only and test files are not part of the original project
list(FILTER OGGVORBIS_SOURCES EXCLUDE REGEX "/(vorbisenc|psytune|tone|barkmel)\.c$")
add_library(oggvorbis STATIC ${OGGVORBIS_SOURCES})
target_include_directories(oggvorbis PUBLIC src/oggvorbis/include PRIVATE src/oggvorbis/vorbis)
target_compile_definitions(oggvorbis PRIVATE _LIB)

###############################################################################
# the game

file(GLOB_RECURSE TANK_SOURCES src/tank/*.cpp src/tank/*.c)
list(FILTER TANK_SOURCES EXCLUDE REGEX "/RenderDirect3D\.cpp$")

add_executable(tank WIN32
	${TANK_SOURCES}
	src/shim/RenderDirect3DStub.cpp
	src/tank/res/tank.rc
)
target_include_directories(tank PRIVATE src/tank src/shim)
target_compile_definitions(tank PRIVATE LOGFILE)
option(TZOD_NETWORK_DEBUG "Per-frame world checksums; the server aborts on the first desync" OFF)
if(TZOD_NETWORK_DEBUG)
	target_compile_definitions(tank PRIVATE NETWORK_DEBUG)
endif()
target_precompile_headers(tank PRIVATE src/tank/stdafx.h)
set_source_files_properties(src/tank/Md5.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_link_libraries(tank PRIVATE lua zlib oggvorbis
	winmm dxguid dsound dinput8 comctl32 ws2_32 opengl32 shell32 user32 gdi32 advapi32 ole32)
set_target_properties(tank PROPERTIES
	OUTPUT_NAME tank
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
