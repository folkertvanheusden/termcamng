# (C) 2020-2023 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
cmake_minimum_required(VERSION 3.9.4)

project(termcamng VERSION 6.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-D_FILE_OFFSET_BITS=64")

#add_definitions("-DLOG_TRAFFIC")

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wall -pedantic)

add_executable(termcamng
	BearSSLHelpers.cpp
	error.cpp
	font.cpp
	http.cpp
	httpd.cpp
	io.cpp
	logging.cpp
	main.cpp
	net.cpp
	net-io.cpp
	net-io-fd.cpp
	picio.cpp
	proc.cpp
	str.cpp
	terminal.cpp
	time.cpp
	utils.cpp
	yaml-helpers.cpp
	)

target_link_libraries(termcamng -lbearssl)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(termcamng Threads::Threads)

include(FindPkgConfig)

pkg_check_modules(LIBPNG REQUIRED libpng)
target_link_libraries(termcamng ${LIBPNG_LIBRARIES})
target_include_directories(termcamng PUBLIC ${LIBPNG_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${LIBPNG_CFLAGS_OTHER})

pkg_check_modules(YAML REQUIRED yaml-cpp)
target_link_libraries(termcamng ${YAML_LIBRARIES})
target_include_directories(termcamng PUBLIC ${YAML_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${YAML_CFLAGS_OTHER})

pkg_check_modules(LIBSSH REQUIRED libssh)
target_link_libraries(termcamng ${LIBSSH_LIBRARIES})
target_include_directories(termcamng PUBLIC ${LIBSSH_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${LIBSSH_CFLAGS_OTHER})

pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
target_link_libraries(termcamng ${FONTCONFIG_LIBRARIES})
target_include_directories(termcamng PUBLIC ${FONTCONFIG_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${FONTCONFIG_CFLAGS_OTHER})

pkg_check_modules(FREETYPE2 REQUIRED freetype2)
target_link_libraries(termcamng ${FREETYPE2_LIBRARIES})
target_include_directories(termcamng PUBLIC ${FREETYPE2_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${FREETYPE2_CFLAGS_OTHER})

pkg_check_modules(LIBJPEG REQUIRED libturbojpeg)
target_link_libraries(termcamng ${LIBJPEG_LIBRARIES})
target_include_directories(termcamng PUBLIC ${LIBJPEG_INCLUDE_DIRS})
target_compile_options(termcamng PUBLIC ${LIBJPEG_CFLAGS_OTHER})

CHECK_INCLUDE_FILE(security/pam_appl.h LIBPAM)
if (LIBPAM)
	target_link_libraries(termcamng pam)
else()
	message(STATUS "No pam library")
endif()

target_link_libraries(termcamng -lutil)
