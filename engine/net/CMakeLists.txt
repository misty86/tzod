cmake_minimum_required (VERSION 3.3)

add_library(net
	inc/net/NetBuffer.h
	inc/net/NetHost.h

	NetBuffer.cpp
	NetHost.cpp
)

target_link_libraries(net PRIVATE enet)
target_include_directories(net INTERFACE inc)
set_target_properties(net PROPERTIES FOLDER engine)
