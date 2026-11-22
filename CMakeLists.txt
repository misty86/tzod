cmake_minimum_required (VERSION 3.6)
project(tzod VERSION 1.52)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(APPLE OR UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
endif()

if((NOT IOS) AND (NOT WINRT) AND (NOT ANDROID))
	set(WITH_NETWORK ON) # ENet based multiplayer, desktop only
endif()

add_subdirectory(external)
add_subdirectory(engine)
add_subdirectory(game)
