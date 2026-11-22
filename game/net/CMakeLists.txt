cmake_minimum_required (VERSION 3.3)

add_library(netgame
	inc/net/Lockstep.h
	inc/net/NetProtocol.h
	inc/net/NetSession.h
	inc/net/NetworkGameContext.h
	inc/net/WorldChecksum.h

	Lockstep.cpp
	NetProtocol.cpp
	NetSession.cpp
	NetworkGameContext.cpp
	WorldChecksum.cpp
)

target_link_libraries(netgame
	PUBLIC ctx net
	PRIVATE ai fs gc plat
)

target_include_directories(netgame INTERFACE inc)
set_target_properties(netgame PROPERTIES FOLDER game)
