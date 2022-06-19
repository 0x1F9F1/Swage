cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/CMakePCHCompiler)

project(Swage CXX CXXPCH)

add_compile_definitions($<$<CONFIG:MinSizeRel,Release>:SWAGE_NO_ASSERTIONS>)
add_compile_definitions($<$<CONFIG:MinSizeRel,Release>:SWAGE_RELEASE>)

add_subdirectory(vendor EXCLUDE_FROM_ALL)

if (MSVC)
    add_compile_options(/W4 /MP /permissive- /Zc:throwingNew /utf-8 $<$<CONFIG:Debug>:/Ob1> $<$<CONFIG:MinSizeRel,Release>:/Ob2>)

    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_compile_options(/arch:SSE2)
    endif()
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /IGNORE:4075")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(SWAGE_ASSET_DIR "${CMAKE_CURRENT_LIST_DIR}/assets" ABSOLUTE)

add_definitions("-DSWAGE_ASSET_DIR=\"${SWAGE_ASSET_DIR}/\"")

add_subdirectory(src)

set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT ArchiveExplorer)
