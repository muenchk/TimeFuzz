cmake_minimum_required(VERSION 3.22)

project(
	TimeFuzz
	VERSION 0.0.1
	LANGUAGES CXX
)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_path(RAPIDCSV_INCLUDE_DIRS "rapidcsv.h")
find_package(Boost
        MODULE
        REQUIRED
        COMPONENTS
        stacktrace_windbg
)
find_package(directxtk CONFIG REQUIRED)
find_package(frozen REQUIRED CONFIG)
find_package(infoware REQUIRED CONFIG)
find_package(magic_enum CONFIG REQUIRED)
find_package(zycore CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)


macro(set_from_environment VARIABLE)
	if(NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif()
endmacro()

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

add_subdirectory(src)
if(BUILD_TESTS STREQUAL "ON")
	enable_testing()
	add_subdirectory(tests)
endif()
include(cmake/packaging.cmake)
