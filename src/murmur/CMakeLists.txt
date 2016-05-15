# Copyright 2005-2016 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

cmake_minimum_required(VERSION 2.8.11)

project(murmur)

set(CMAKE_PREFIX_PATH "/Users/mkrautz/QtDist/5.6/clang_64" ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "/MumbleBuild/osx-1.3.x-2016-05-06-cb41a91-634" ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "/MumbleBuild/osx-1.3.x-2016-05-06-cb41a91-634/Ice-3.4.2" ${CMAKE_PREFIX_PATH})
set(Ice_SLICE_DIR "/MumbleBuild/osx-1.3.x-2016-05-06-cb41a91-634/Ice-3.4.2/slice/" ${CMAKE_PREFIX_PATH})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED Core Network Sql Xml)
find_package(Boost REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)

find_package(Ice OPTIONAL_COMPONENTS Ice IceUtil)

include_directories(
	.
	..
)

set(SOURCES
	../ACL.h
	../Channel.h
	../CryptState.h
	../Connection.h
	../Group.h
	../HTMLFilter.h
	../User.h
	../Net.h
	../OSInfo.h
	../Timer.h
	../SSL.h
	../Version.h
	../SSLCipherInfo.h
	../SSLCipherInfoTable.h

	Server.h
	ServerUser.h
	Meta.h
	PBKDF2.h

	../ACL.cpp
	../Group.cpp
	../Channel.cpp
	../Connection.cpp
	../HTMLFilter.cpp
	../User.cpp
	../Timer.cpp
	../CryptState.cpp
	../OSInfo.cpp
	../Net.cpp
	../SSL.cpp
	../Version.cpp
	../SSLCipherInfo.cpp

	main.cpp
	Server.cpp
	ServerUser.cpp
	ServerDB.cpp
	Register.cpp
	Cert.cpp
	Messages.cpp
	Meta.cpp
	RPC.cpp
	PBKDF2.cpp
	UnixMurmur.cpp # XXX: unix only

	murmur.qrc
)

if (Ice_FOUND)
	set (SOURCES
		
		# XXX: generated out of tree for now
		Murmur.cpp
		Murmur.h

		MurmurI.h

		MurmurIce.cpp
		MurmurIce.h

		${SOURCES}
	)
endif()

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenSSL_INCLUDE_DIRS})

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ../Mumble.proto)

add_executable(murmur ${SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(murmur Qt5::Core Qt5::Network Qt5::Sql Qt5::Xml ${PROTOBUF_LIBRARIES} ${OPENSSL_LIBRARIES})
if (Ice_FOUND)
	include_directories(murmur ${Ice_INCLUDE_DIRS})
	target_link_libraries(murmur ${Ice_LIBRARIES})
	target_compile_definitions(murmur PRIVATE USE_ICE)
endif()
target_link_libraries(murmur "-framework ApplicationServices")
target_link_libraries(murmur "-framework CoreFoundation")
target_link_libraries(murmur "-framework Foundation")
target_link_libraries(murmur "-framework Security")
target_compile_definitions(murmur PRIVATE MURMUR)
