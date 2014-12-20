PROJECT(knowson)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)

SET(SOURCES
	"definition.cc"
	"definition.h"
	"json.cc"
	"json.h"
	"json_parser.cc"
	"json_parser.h")

ADD_DEFINITIONS(-std=c++11)
ADD_EXECUTABLE(knowson ${SOURCES})