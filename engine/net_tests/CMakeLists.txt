cmake_minimum_required (VERSION 3.3)
project(NetTests)

add_executable(net_tests
	NetBuffer_tests.cpp
	NetHost_tests.cpp
)

target_link_libraries(net_tests PRIVATE
	net
	gtest_main
)

target_include_directories(net_tests PRIVATE
	${gtest_SOURCE_DIR}/include
)
set_target_properties(net_tests PROPERTIES FOLDER engine)
