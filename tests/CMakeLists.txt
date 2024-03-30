set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

include_directories("${ROOT_DIR}/include")
set(TEST_SOURCE_DIR "${ROOT_DIR}/tests")
set(SOURCE_DIR "${ROOT_DIR}/src")
include(${ROOT_DIR}/cmake/sourcelist.cmake)

configure_file(
	"${ROOT_DIR}/cmake/version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	@ONLY
)

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
		cxx_std_23
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
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

	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}/src"
		"${SOURCE_DIR}"
		${fmt_INCLUDE_DIRS}
		${spdlog_INCLUDE_DIRS}
		${RAPIDCSV_INCLUDE_DIRS}
)


add_executable(
	"TaskController_Test"
	"${TEST_SOURCE_DIR}/TaskController_Test.cpp"
	${SOURCE_FILES}
	"${VERSION_HEADER}"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	"${ROOT_DIR}/.clang-format"
	"${ROOT_DIR}/.editorconfig"
)

add_test(NAME TaskController COMMAND $<TARGET_FILE:TaskController_Test>)
