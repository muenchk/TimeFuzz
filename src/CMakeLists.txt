set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

include_directories("${ROOT_DIR}/include")
include_directories("${ROOT_DIR}/CrashHandler")
include_directories("${ROOT_DIR}/DeathHandler")

set(SOURCE_DIR "${ROOT_DIR}/src")
include(${ROOT_DIR}/cmake/sourcelist.cmake)
#include_directories("D:/Program Files/Microsoft Visual Studio/2022/DIA SDK/include")

include(CheckIncludeFileCXX)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "Clang;MSVC")
else()
	#CHECK_INCLUDE_FILE_CXX(libexplain/execvp.h LIBEXPLAIN_INCLUDE)
#	find_library(LIBEXPLAIN_INCLUDE explain libexplain)
#	if (LIBEXPLAIN_INCLUDE)
#        message(STATUS "Found libexplain")
#		add_compile_definitions(-DINCLLIBEXPLAIN)
#	else()
#        message(STATUS "Cannot find libexplain")
#	endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "Clang;MSVC")

	set(VS_PATH ${CMAKE_GENERATOR_INSTANCE})
	get_filename_component(VS_DIA_INC_PATH "${VS_PATH}/DIA SDK/include" ABSOLUTE CACHE)

	find_path(DIASDK_INCLUDE_DIR # Set variable DIASDK_INCLUDE_DIR
		dia2.h # Find a path with dia2.h
		HINTS "${VS_DIA_INC_PATH}"
		HINTS "${VSWHERE_LATEST}/DIA SDK/include"
		DOC "path to DIA SDK header files"
	)
	include_directories(${DIASDK_INCLUDE_DIR})
	# VS 2017
	if(CMAKE_GENERATOR MATCHES "Visual Studio 15 2017.*")
		if(CMAKE_GENERATOR MATCHES "Visual Studio.*Win64")
		        find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/amd64)
		elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM")
		        find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/arm)
		else(CMAKE_GENERATOR MATCHES "Visual Studio.*Win64")
		        find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib)
		endif(CMAKE_GENERATOR MATCHES "Visual Studio.*Win64")
	else(CMAKE_GENERATOR MATCHES "Visual Studio 15 2017.*")
		if((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR("${DXC_BUILD_ARCH}" STREQUAL "x64"))
		        find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/amd64)
		elseif((CMAKE_GENERATOR_PLATFORM STREQUAL "ARM") OR("${DXC_BUILD_ARCH}" STREQUAL "ARM"))
		        find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/arm)
		elseif((CMAKE_GENERATOR_PLATFORM MATCHES "ARM64.*") OR("${DXC_BUILD_ARCH}" MATCHES "ARM64.*"))
		        find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/arm64)
		else((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR("${DXC_BUILD_ARCH}" STREQUAL "x64"))
		        find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib)
		endif((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR("${DXC_BUILD_ARCH}" STREQUAL "x64"))
	endif(CMAKE_GENERATOR MATCHES "Visual Studio 15 2017.*")

	set(DIASDK_LIBRARIES ${DIASDK_GUIDS_LIBRARY})
	set(DIASDK_INCLUDE_DIRS ${DIASDK_INCLUDE_DIR})
	set(DIA_DLL "${DIASDK_INCLUDE_DIRS}/../bin/amd64/msdia140.dll")
	# handle the QUIETLY and REQUIRED arguments and set DIASDK_FOUND to TRUE
	# if all listed variables are TRUE
	find_package_handle_standard_args(DiaSDK DEFAULT_MSG
		DIASDK_LIBRARIES DIASDK_INCLUDE_DIR)
	mark_as_advanced(DIASDK_INCLUDE_DIRS DIASDK_LIBRARIES)
endif()

source_group(TREE "${ROOT_DIR}" FILES ${SOURCE_FILES})

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/Plugin.h")
configure_file(
	"${ROOT_DIR}/cmake/Plugin.h.in"
	"${VERSION_HEADER}"
	@ONLY
)

source_group("src" FILES "${VERSION_HEADER}")

configure_file(
	"${ROOT_DIR}/cmake/version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	@ONLY
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "Clang;MSVC")
	add_compile_options("$<$<NOT:$<CONFIG:Debug>>:/Zi>")
	add_link_options("$<$<NOT:$<CONFIG:Debug>>:/DEBUG>")
	add_link_options("$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>")
	add_link_options("$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	add_compile_options(-std=gnu++23)
	add_link_options( -std=gnu++23)
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "Clang;MSVC")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	
endif()

if ("${EnableUI}" STREQUAL "ON")
	add_compile_definitions(EnableUI)
endif()

add_library(
	"${PROJECT_NAME}_lib"
	STATIC
	${SOURCE_FILES}
	"${VERSION_HEADER}"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	"${ROOT_DIR}/.clang-format"
	"${ROOT_DIR}/.editorconfig")

if ("${DISABLE_CUSTOM_ALLOCATORS}" STREQUAL "ON")
	add_compile_definitions(CUSTOM_ALLOCATORS)
	message(STATUS "Building with custom allocators")
endif()

add_executable("${PROJECT_NAME}" "${SOURCE_DIR}/main.cpp" "${VERSION_HEADER}" "${CMAKE_CURRENT_BINARY_DIR}/version.rc" "${ROOT_DIR}/.clang-format" "${ROOT_DIR}/.editorconfig")

if(DIASDK_LIBRARIES)
        add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${DIA_DLL} "./")
endif()

if(DIASDK_LIBRARIES)
        target_include_directories(${PROJECT_NAME}
                PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                ${DIASDK_INCLUDE_DIRS}
                ${DIASDK_INCLUDE_DIRS}/../lib
        )
        target_link_libraries(${PROJECT_NAME}
                PUBLIC
                ${DIASDK_INCLUDE_DIRS}/../lib/amd64/diaguids.lib
        )
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "Clang;MSVC")
	if ("${EnableUI}" STREQUAL "ON")
		target_link_libraries(
			"${PROJECT_NAME}"
			PRIVATE
			${PROJECT_NAME}_lib
		)
		target_link_libraries(
			"${PROJECT_NAME}_lib"
			PRIVATE
			fmt::fmt
			lua
			CrashHandler
			imgui::imgui
			glfw
			glad::glad
			glm::glm
			liblzma::liblzma
		)
	else()
		target_link_libraries(
			"${PROJECT_NAME}"
			PRIVATE
			fmt::fmt
			lua
			CrashHandler
			liblzma::liblzma
			${PROJECT_NAME}_lib
		)
		target_link_libraries(
			"${PROJECT_NAME}_lib"
			PRIVATE
			fmt::fmt
			lua
			CrashHandler
			liblzma::liblzma
		)
	endif()
else()
	add_compile_options(-g)
	if ("${EnableUI}" STREQUAL "ON")
		target_link_libraries(
			"${PROJECT_NAME}"
			PRIVATE
			fmt::fmt
			lua
			c
			imgui::imgui
			glfw
			glad::glad
			glm::glm
			liblzma::liblzma
			${PROJECT_NAME}_lib
		)
		add_compile_options(add_compile_options("$<$<CONFIG:Debug>:-fsanitize=address -fno-omit-frame-pointer>"))
		if (LIBEXPLAIN_INCLUDE)
			target_link_libraries(
				"${PROJECT_NAME}_lib"
				PRIVATE
				fmt::fmt
				lua
				Threads::Threads
				pthread
				explain
				c
				imgui::imgui
				glfw
				glad::glad
				glm::glm
				liblzma::liblzma
				Boost::thread
			)
		else()
			target_link_libraries(
				"${PROJECT_NAME}_lib"
				PRIVATE
				fmt::fmt
				lua
				Threads::Threads
				pthread
				c
				imgui::imgui
				glfw
				glad::glad
				glm::glm
				liblzma::liblzma
				Boost::thread
			)
		endif()
	else()
		target_link_libraries(
			"${PROJECT_NAME}"
			PRIVATE
			fmt::fmt
			lua
			c
			liblzma::liblzma
			${PROJECT_NAME}_lib
		)
		add_compile_options(add_compile_options("$<$<CONFIG:Debug>:-fsanitize=address -fno-omit-frame-pointer>"))
		if (LIBEXPLAIN_INCLUDE)
			target_link_libraries(
				"${PROJECT_NAME}_lib"
				PRIVATE
				fmt::fmt
				lua
				Threads::Threads
				pthread
				explain
				c
				liblzma::liblzma
				Boost::thread
			)
		else()
			target_link_libraries(
				"${PROJECT_NAME}_lib"
				PRIVATE
				fmt::fmt
				lua
				Threads::Threads
				pthread
				c
				liblzma::liblzma
				Boost::thread
			)
		endif()
	endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "Clang;MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
			"/sdl"	# Enable Additional Security Checks
			"/utf-8"	# Set Source and Executable character sets to UTF-8
			"/Zi"	# Debug Information Format

			"/permissive-"	# Standards conformance
			"/Zc:preprocessor"	# Enable preprocessor conformance mode

			"/D_CRT_SECURE_NO_WARNINGS"

			"/wd5105"
			# disable warnings
			"/wd4189"
			"/wd4005" # macro redefinition
			"/wd4061" # enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label
			"/wd4200" # nonstandard extension used : zero-sized array in struct/union
			"/wd4201" # nonstandard extension used : nameless struct/union
			"/wd4265" # 'type': class has virtual functions, but its non-trivial destructor is not virtual; instances of this class may not be destructed correctly
			"/wd4266" # 'function' : no override available for virtual member function from base 'type'; function is hidden
			"/wd4371" # 'classname': layout of class may have changed from a previous version of the compiler due to better packing of member 'member'
			"/wd4514" # 'function' : unreferenced inline function has been removed
			"/wd4582" # 'type': constructor is not implicitly called
			"/wd4583" # 'type': destructor is not implicitly called
			"/wd4623" # 'derived class' : default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
			"/wd4625" # 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
			"/wd4626" # 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
			"/wd4710" # 'function' : function not inlined
			"/wd4711" # function 'function' selected for inline expansion
			"/wd4820" # 'bytes' bytes padding added after construct 'member_name'
			"/wd5026" # 'type': move constructor was implicitly defined as deleted
			"/wd5027" # 'type': move assignment operator was implicitly defined as deleted
			"/wd5045" # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
			"/wd5053" # support for 'explicit(<expr>)' in C++17 and earlier is a vendor extension
			"/wd5204" # 'type-name': class has virtual functions, but its trivial destructor is not virtual; instances of objects derived from this class may not be destructed correctly
			"/wd5220" # 'member': a non-static data member with a volatile qualified type no longer implies that compiler generated copy / move constructors and copy / move assignment operators are not trivial
			#"/wd4333" # to large right shift -> data loss

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)
	target_compile_options(
		"${PROJECT_NAME}_lib"
		PRIVATE
			"/sdl"	# Enable Additional Security Checks
			"/utf-8"	# Set Source and Executable character sets to UTF-8
			"/Zi"	# Debug Information Format

			"/permissive-"	# Standards conformance
			"/Zc:preprocessor"	# Enable preprocessor conformance mode

			"/D_CRT_SECURE_NO_WARNINGS"

			"/wd5105"
			# disable warnings
			"/wd4189"
			"/wd4005" # macro redefinition
			"/wd4061" # enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label
			"/wd4200" # nonstandard extension used : zero-sized array in struct/union
			"/wd4201" # nonstandard extension used : nameless struct/union
			"/wd4265" # 'type': class has virtual functions, but its non-trivial destructor is not virtual; instances of this class may not be destructed correctly
			"/wd4266" # 'function' : no override available for virtual member function from base 'type'; function is hidden
			"/wd4371" # 'classname': layout of class may have changed from a previous version of the compiler due to better packing of member 'member'
			"/wd4514" # 'function' : unreferenced inline function has been removed
			"/wd4582" # 'type': constructor is not implicitly called
			"/wd4583" # 'type': destructor is not implicitly called
			"/wd4623" # 'derived class' : default constructor was implicitly defined as deleted because a base class default constructor is inaccessible or deleted
			"/wd4625" # 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
			"/wd4626" # 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
			"/wd4710" # 'function' : function not inlined
			"/wd4711" # function 'function' selected for inline expansion
			"/wd4820" # 'bytes' bytes padding added after construct 'member_name'
			"/wd5026" # 'type': move constructor was implicitly defined as deleted
			"/wd5027" # 'type': move assignment operator was implicitly defined as deleted
			"/wd5045" # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
			"/wd5053" # support for 'explicit(<expr>)' in C++17 and earlier is a vendor extension
			"/wd5204" # 'type-name': class has virtual functions, but its trivial destructor is not virtual; instances of objects derived from this class may not be destructed correctly
			"/wd5220" # 'member': a non-static data member with a volatile qualified type no longer implies that compiler generated copy / move constructors and copy / move assignment operators are not trivial
			#"/wd4333" # to large right shift -> data loss

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
else()
	add_compile_options(-g)
#else()
#	target_compile_options(
#		"${PROJECT_NAME}"
#		PRIVATE
#		"-std=c++23")
#	target_compile_options(
#		"${PROJECT_NAME}_lib"
#		PRIVATE
#		"-std=c++23")
endif()

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}/src"
		"${SOURCE_DIR}"
		${fmt_INCLUDE_DIRS}
		${spdlog_INCLUDE_DIRS}
		${RAPIDCSV_INCLUDE_DIRS}
		${infoware_INCLUDE_DIRS}
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "Clang;MSVC")
	install(
		FILES
			"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
		DESTINATION "/"
		COMPONENT "pdbs"
	)
endif()

file(COPY "${ROOT_DIR}/static/background.png" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${ROOT_DIR}/static/shader/4.1.texture.fs" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shader")
file(COPY "${ROOT_DIR}/static/shader/4.1.texture.vs" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shader")

