# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# detect AppleClang; needs to come before project()
cmake_policy(SET CMP0025 NEW)

project(libfido2 C)
cmake_minimum_required(VERSION 3.0)

include(CheckCCompilerFlag)
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)
include(GNUInstallDirs)

set(CMAKE_COLOR_MAKEFILE off)
set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FIDO_MAJOR "1")
set(FIDO_MINOR "3")
set(FIDO_PATCH "1")
set(FIDO_VERSION ${FIDO_MAJOR}.${FIDO_MINOR}.${FIDO_PATCH})

add_definitions(-D_FIDO_MAJOR=${FIDO_MAJOR})
add_definitions(-D_FIDO_MINOR=${FIDO_MINOR})
add_definitions(-D_FIDO_PATCH=${FIDO_PATCH})

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

if(APPLE)
	set(CMAKE_INSTALL_NAME_DIR
		"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()

# /dev/urandom
if(UNIX)
	add_definitions(-DHAS_DEV_URANDOM)
endif()

# Observe OpenBSD's library versioning scheme.
if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	set(LIB_VERSION ${FIDO_MAJOR}.${FIDO_MINOR})
	set(LIB_SOVERSION ${LIB_VERSION})
else()
	set(LIB_VERSION ${FIDO_VERSION})
	set(LIB_SOVERSION ${FIDO_MAJOR})
endif()

if(MSVC)
	if((NOT CBOR_INCLUDE_DIRS) OR (NOT CBOR_LIBRARY_DIRS) OR
	   (NOT CRYPTO_INCLUDE_DIRS) OR (NOT CRYPTO_LIBRARY_DIRS))
		message(FATAL_ERROR "please provide definitions for "
		    "{CBOR,CRYPTO}_{INCLUDE,LIBRARY}_DIRS when building "
		    "under msvc")
	endif()
	set(CBOR_LIBRARIES cbor)
	set(CRYPTO_LIBRARIES crypto-45)
	set(MSVC_DISABLED_WARNINGS_LIST
		"C4200" # nonstandard extension used: zero-sized array in
			# struct/union;
		"C4204" # nonstandard extension used: non-constant aggregate
			# initializer;
		"C4706" # assignment within conditional expression;
		"C4996" # The POSIX name for this item is deprecated. Instead,
			# use the ISO C and C++ conformant name
		)
	# The construction in the following 3 lines was taken from LibreSSL's
	# CMakeLists.txt.
	string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
		${MSVC_DISABLED_WARNINGS_LIST})
	string(REGEX REPLACE "[/-]W[1234][ ]?" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MP -W4 ${MSVC_DISABLED_WARNINGS_STR}")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Z7")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")
else()
	include(FindPkgConfig)
	pkg_search_module(CBOR libcbor)
	pkg_search_module(CRYPTO libcrypto REQUIRED)

	# XXX workaround libcbor's missing .pc file
	if(NOT CBOR_FOUND)
		check_include_files(cbor.h HAVE_CBOR_H)
		if(NOT HAVE_CBOR_H)
			message(FATAL_ERROR "could not find cbor header files")
		endif()
		set(CBOR_LIBRARIES "cbor")
	endif()

	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		pkg_search_module(UDEV libudev REQUIRED)
		set(UDEV_NAME "udev")
		# Define be32toh().
		add_definitions(-D_GNU_SOURCE)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
		set(BASE_LIBRARIES usbhid)
	endif()

	if(MINGW)
		# MinGW is stuck with a flavour of C89.
		add_definitions(-DFIDO_NO_DIAGNOSTIC)
		add_definitions(-DWC_ERR_INVALID_CHARS=0x80)
	endif()

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwrite-strings")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wbad-function-cast")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic-errors")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g2")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")

	if(FUZZ)
		if(LIBFUZZER)
			set(FUZZ_LDFLAGS "-fsanitize=fuzzer")
		endif()
		add_definitions(-DFIDO_FUZZ)
	endif()

	if(ASAN)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,leak")
	endif()

	if(MSAN)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-memory-track-origins")
	endif()

	if(UBSAN)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-trap=undefined")
	endif()

	if(COVERAGE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
	endif()
endif()

# Use -Wshorten-64-to-32 if available.
check_c_compiler_flag("-Wshorten-64-to-32" HAVE_SHORTEN_64_TO_32)
if(HAVE_SHORTEN_64_TO_32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshorten-64-to-32")
endif()

# Avoid https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result")
endif()

# Decide which keyword to use for thread-local storage.
if(CMAKE_COMPILER_IS_GNUCC OR
   CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
	set(TLS "__thread")
elseif(WIN32)
	set(TLS "__declspec(thread)")
endif()

add_definitions(-DTLS=${TLS})

# endian.h
check_include_files(endian.h HAVE_ENDIAN_H)
if(HAVE_ENDIAN_H)
	add_definitions(-DHAVE_ENDIAN_H)
endif()

# err.h
check_include_files(err.h HAVE_ERR_H)
if(HAVE_ERR_H)
	add_definitions(-DHAVE_ERR_H)
endif()

# unistd.h
check_include_files(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
	add_definitions(-DHAVE_UNISTD_H)
endif()

# signal.h
check_include_files(signal.h HAVE_SIGNAL_H)
if(HAVE_SIGNAL_H)
	add_definitions(-DHAVE_SIGNAL_H)
endif()

# strlcpy
check_function_exists(strlcpy HAVE_STRLCPY)
if(HAVE_STRLCPY)
	add_definitions(-DHAVE_STRLCPY)
endif()

# strlcat
check_function_exists(strlcpy HAVE_STRLCAT)
if(HAVE_STRLCAT)
	add_definitions(-DHAVE_STRLCAT)
endif()

# recallocarray
check_function_exists(recallocarray HAVE_RECALLOCARRAY)
if(HAVE_RECALLOCARRAY)
	add_definitions(-DHAVE_RECALLOCARRAY)
endif()

# XXX getpagesize is incorrectly detected when cross-compiling
# with mingw on Linux. Avoid.
if(NOT WIN32)
	check_function_exists(getpagesize HAVE_GETPAGESIZE)
endif()
if(HAVE_GETPAGESIZE)
	add_definitions(-DHAVE_GETPAGESIZE)
endif()

# sysconf
check_function_exists(sysconf HAVE_SYSCONF)
if(HAVE_SYSCONF)
	add_definitions(-DHAVE_SYSCONF)
endif()

# memset_s
if(APPLE)
	add_definitions(-D__STDC_WANT_LIB_EXT1__=1)
endif()
check_function_exists(memset_s HAVE_MEMSET_S)
if(HAVE_MEMSET_S)
	add_definitions(-DHAVE_MEMSET_S)
endif()

# explicit_bzero
if(NOT LIBFUZZER)
	check_function_exists(explicit_bzero HAVE_EXPLICIT_BZERO)
	if(HAVE_EXPLICIT_BZERO)
		add_definitions(-DHAVE_EXPLICIT_BZERO)
	endif()
endif()

# timingsafe_bcmp
check_function_exists(timingsafe_bcmp HAVE_TIMINGSAFE_BCMP)
if(HAVE_TIMINGSAFE_BCMP)
	add_definitions(-DHAVE_TIMINGSAFE_BCMP)
endif()

# readpassphrase
check_function_exists(readpassphrase HAVE_READPASSPHRASE)
if(HAVE_READPASSPHRASE)
	add_definitions(-DHAVE_READPASSPHRASE)
endif()

# getline
check_function_exists(getline HAVE_GETLINE)
if(HAVE_GETLINE)
	add_definitions(-DHAVE_GETLINE)
endif()

# getopt
check_function_exists(getopt HAVE_GETOPT)
if(HAVE_GETOPT)
	add_definitions(-DHAVE_GETOPT)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-qual")
else()
	if(CMAKE_COMPILER_IS_GNUCC)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-discarded-qualifiers")
	endif()
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-incompatible-pointer-types-discards-qualifiers")
	endif()
endif()

# usable sigaction
set(CMAKE_EXTRA_INCLUDE_FILES signal.h)
check_function_exists(sigaction HAVE_SIGACTION)
check_type_size("sig_atomic_t" HAVE_SIG_ATOMIC_T)
if(HAVE_SIGACTION AND (NOT HAVE_SIG_ATOMIC_T STREQUAL ""))
	add_definitions(-DSIGNAL_EXAMPLE)
endif()
set(CMAKE_EXTRA_INCLUDE_FILES)

# arc4random_buf
check_function_exists(arc4random_buf HAVE_ARC4RANDOM_BUF)
if(HAVE_ARC4RANDOM_BUF)
	add_definitions(-DHAVE_ARC4RANDOM_BUF)
endif()

# getentropy
check_function_exists(getentropy HAVE_GETENTROPY)
if(HAVE_GETENTROPY)
	add_definitions(-DHAVE_GETENTROPY)
endif()

# export list
if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
	# clang + lld
	string(CONCAT CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}
	    " -exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/src/export.llvm")
elseif(NOT MSVC)
	# clang/gcc + gnu ld
	string(CONCAT CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}
	    " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/export.gnu")
	if(NOT WIN32)
		string(CONCAT CMAKE_SHARED_LINKER_FLAGS
		    ${CMAKE_SHARED_LINKER_FLAGS}
		    " -Wl,-z,noexecstack -Wl,-z,relro,-z,now")
		string(CONCAT CMAKE_EXE_LINKER_FLAGS
		    ${CMAKE_EXE_LINKER_FLAGS}
		    " -Wl,-z,noexecstack -Wl,-z,relro,-z,now")
		if(FUZZ)
			file(STRINGS fuzz/wrapped.sym WRAPPED_SYMBOLS)
			foreach(s ${WRAPPED_SYMBOLS})
				string(CONCAT CMAKE_SHARED_LINKER_FLAGS
				    ${CMAKE_SHARED_LINKER_FLAGS}
				    " -Wl,--wrap=${s}")
			endforeach()
		endif()
	endif()
else()
	string(CONCAT CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS}
		" /def:\"${CMAKE_CURRENT_SOURCE_DIR}/src/export.msvc\"")
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CBOR_INCLUDE_DIRS})
include_directories(${CRYPTO_INCLUDE_DIRS})

link_directories(${CBOR_LIBRARY_DIRS})
link_directories(${CRYPTO_LIBRARY_DIRS})

message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_C_COMPILER_ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CBOR_INCLUDE_DIRS: ${CBOR_INCLUDE_DIRS}")
message(STATUS "CBOR_LIBRARY_DIRS: ${CBOR_LIBRARY_DIRS}")
message(STATUS "CBOR_LIBRARIES: ${CBOR_LIBRARIES}")
message(STATUS "CRYPTO_INCLUDE_DIRS: ${CRYPTO_INCLUDE_DIRS}")
message(STATUS "CRYPTO_LIBRARY_DIRS: ${CRYPTO_LIBRARY_DIRS}")
message(STATUS "CRYPTO_LIBRARIES: ${CRYPTO_LIBRARIES}")
message(STATUS "BASE_LIBRARIES: ${BASE_LIBRARIES}")
message(STATUS "VERSION: ${FIDO_VERSION}")
message(STATUS "LIB_VERSION: ${LIB_VERSION}")
message(STATUS "LIB_SOVERSION: ${LIB_SOVERSION}")
message(STATUS "FUZZ: ${FUZZ}")
message(STATUS "AFL: ${AFL}")
message(STATUS "LIBFUZZER: ${LIBFUZZER}")
message(STATUS "ASAN: ${ASAN}")
message(STATUS "MSAN: ${MSAN}")
message(STATUS "COVERAGE: ${COVERAGE}")
message(STATUS "TLS: ${TLS}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(STATUS "UDEV_INCLUDE_DIRS: ${UDEV_INCLUDE_DIRS}")
	message(STATUS "UDEV_LIBRARY_DIRS: ${UDEV_LIBRARY_DIRS}")
	message(STATUS "UDEV_LIBRARIES: ${UDEV_LIBRARIES}")
	message(STATUS "UDEV_RULES_DIR: ${UDEV_RULES_DIR}")
endif()

subdirs(src)
subdirs(examples)
subdirs(tools)
subdirs(man)

if(NOT WIN32)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		if(NOT MSAN AND NOT LIBFUZZER)
			subdirs(regress)
		endif()
	endif()
	if(FUZZ)
		subdirs(fuzz)
	endif()

	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		subdirs(udev)
	endif()
endif()
