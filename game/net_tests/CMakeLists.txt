cmake_minimum_required (VERSION 3.3)
project(NetGameTests)

add_executable(netgame_tests
	Lockstep_tests.cpp
	NetworkGame_tests.cpp
	Spawn_tests.cpp
)

if(WIN32)
	set(NETGAME_TESTS_FSLIB fswin)
else()
	set(NETGAME_TESTS_FSLIB fsposix)
endif()

# the game object classes register themselves from static initializers, so the
# whole gc library must be linked or the map loader will silently skip unknown types
target_link_libraries(netgame_tests PRIVATE
	as
	ctx
	"$<LINK_LIBRARY:WHOLE_ARCHIVE,gc>"
	fs
	${NETGAME_TESTS_FSLIB}
	netgame
	plat
	gtest_main
)

target_compile_definitions(netgame_tests PRIVATE TZOD_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
target_include_directories(netgame_tests PRIVATE
	${gtest_SOURCE_DIR}/include
)
set_target_properties(netgame_tests PROPERTIES FOLDER game)
