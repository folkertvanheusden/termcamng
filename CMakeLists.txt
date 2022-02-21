# (C) 2020-2021 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
cmake_minimum_required(VERSION 3.9.4)

project(termcamng VERSION 6.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-D_FILE_OFFSET_BITS=64")

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wall -pedantic)

add_executable(termcamng
	error.cpp
	font.cpp
	io.cpp
	main.cpp
	picio.cpp
	proc.cpp
	str.cpp
	terminal.cpp
	time.cpp
	yaml-helpers.cpp
	)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(termcamng Threads::Threads)

include(FindPkgConfig)

pkg_check_modules(LIBPNG REQUIRED libpng)
target_link_libraries(termcamng ${LIBPNG_LIBRARIES})
target_include_directories(termcamng PUBLIC ${LIBPNG_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${LIBPNG_CFLAGS_OTHER})

pkg_check_modules(LIBMICROHTTPD REQUIRED libmicrohttpd)
target_link_libraries(termcamng ${LIBMICROHTTPD_LIBRARIES})
target_include_directories(termcamng PUBLIC ${LIBMICROHTTPD_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${LIBMICROHTTPD_CFLAGS_OTHER})

pkg_check_modules(YAML REQUIRED yaml-cpp)
target_link_libraries(termcamng ${YAML_LIBRARIES})
target_include_directories(termcamng PUBLIC ${YAML_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${YAML_CFLAGS_OTHER})

target_link_libraries(termcamng -lutil)
