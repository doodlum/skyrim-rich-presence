cmake_minimum_required(VERSION 3.21)

project(
	DiscordRichPresence
	VERSION 1.0.0
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(XSEPlugin)

find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c")

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		${SIMPLEINI_INCLUDE_DIRS}
)

find_package(magic_enum CONFIG REQUIRED)

target_link_libraries(
${PROJECT_NAME} 
PRIVATE
	magic_enum::magic_enum
)