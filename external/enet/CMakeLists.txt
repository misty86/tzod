cmake_minimum_required (VERSION 3.3)

# ENet 1.3.18 - reliable UDP networking library (MIT license, see LICENSE)
add_library(enet STATIC
	include/enet/callbacks.h
	include/enet/enet.h
	include/enet/list.h
	include/enet/protocol.h
	include/enet/time.h
	include/enet/types.h
	include/enet/unix.h
	include/enet/utility.h
	include/enet/win32.h

	callbacks.c
	compress.c
	host.c
	list.c
	packet.c
	peer.c
	protocol.c
	unix.c
	win32.c
)

target_include_directories(enet PUBLIC include)
target_compile_definitions(enet PRIVATE ENET_BUILDING_LIB)

if(WIN32)
	target_link_libraries(enet PUBLIC ws2_32 winmm)
	target_compile_definitions(enet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
	include(CheckFunctionExists)
	include(CheckStructHasMember)
	include(CheckTypeSize)
	check_function_exists("fcntl" HAS_FCNTL)
	check_function_exists("poll" HAS_POLL)
	check_function_exists("getaddrinfo" HAS_GETADDRINFO)
	check_function_exists("getnameinfo" HAS_GETNAMEINFO)
	check_function_exists("gethostbyname_r" HAS_GETHOSTBYNAME_R)
	check_function_exists("gethostbyaddr_r" HAS_GETHOSTBYADDR_R)
	check_function_exists("inet_pton" HAS_INET_PTON)
	check_function_exists("inet_ntop" HAS_INET_NTOP)
	check_struct_has_member("struct msghdr" "msg_flags" "sys/types.h;sys/socket.h" HAS_MSGHDR_FLAGS)
	set(CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" "sys/socket.h")
	check_type_size("socklen_t" HAS_SOCKLEN_T BUILTIN_TYPES_ONLY)
	unset(CMAKE_EXTRA_INCLUDE_FILES)
	foreach(def HAS_FCNTL HAS_POLL HAS_GETADDRINFO HAS_GETNAMEINFO HAS_GETHOSTBYNAME_R HAS_GETHOSTBYADDR_R HAS_INET_PTON HAS_INET_NTOP HAS_MSGHDR_FLAGS HAS_SOCKLEN_T)
		if(${def})
			target_compile_definitions(enet PRIVATE ${def}=1)
		endif()
	endforeach()
endif()
