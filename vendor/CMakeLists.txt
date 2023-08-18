project(ThirdParty C CXX)

set(SDL2_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.22/include" CACHE PATH "SDL2 Include Directories")

set(ZSTD_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/zstd-v1.5.5/include" CACHE PATH "ZSTD Include Directories")

if(${CMAKE_SIZEOF_VOID_P} MATCHES 8)
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.22/lib/x64/SDL2.lib;" CACHE PATH "SDL2 Library Directories")
    set(SDL2MAIN_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.22/lib/x64/SDL2main.lib" CACHE PATH "SDL2main Library Directories")
    set(SDL2_BINARIES "${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.22/lib/x64/SDL2.dll" CACHE PATH "SDL2 Binary Directories")

    set(ZSTD_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/zstd-v1.5.5/lib/x64/libzstd.lib;" CACHE PATH "Zstd Library Directories")
    set(ZSTD_BINARIES "${CMAKE_CURRENT_LIST_DIR}/zstd-v1.5.5/lib/x64/libzstd.dll;" CACHE PATH "Zstd Binary Directories")
else()
    set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.22/lib/x86/SDL2.lib;" CACHE PATH "SDL2 Library Directories")
    set(SDL2MAIN_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.22/lib/x86/SDL2main.lib" CACHE PATH "SDL2 Library Directories")
    set(SDL2_BINARIES "${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.22/lib/x86/SDL2.dll" CACHE PATH "SDL2 Binary Directories")

    set(ZSTD_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/zstd-v1.5.5/lib/x86/libzstd.lib;" CACHE PATH "Zstd Library Directories")
    set(ZSTD_BINARIES "${CMAKE_CURRENT_LIST_DIR}/zstd-v1.5.5/lib/x86/libzstd.dll;" CACHE PATH "Zstd Binary Directories")
endif()

add_library(SDL2 SHARED IMPORTED GLOBAL)
set_target_properties(SDL2 PROPERTIES
    IMPORTED_IMPLIB "${SDL2_LIBRARIES}"
    IMPORTED_LOCATION "${SDL2_BINARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")

add_library(SDL2main STATIC IMPORTED GLOBAL)
set_target_properties(SDL2main PROPERTIES
    IMPORTED_LOCATION "${SDL2MAIN_LIBRARIES}")

add_subdirectory(fmt-8.1.1)

add_subdirectory(imgui-1.87)

set(WITH_GZFILEOP OFF CACHE BOOL "")
set(ZLIB_ENABLE_TESTS OFF CACHE BOOL "")
set(FORCE_SSE2 ON CACHE BOOL "")
add_subdirectory(zlib-ng-2.0.6)

add_subdirectory(wolfssl-4.2.0)
target_compile_definitions(wolfcrypt PUBLIC WC_NO_HARDEN WOLFSSL_AES_DIRECT HAVE_AES_ECB)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "")
set(YAML_CPP_INSTALL OFF CACHE BOOL "")
add_subdirectory(yaml-cpp-0.7.0)

file(GLOB LUA_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/lua-5.4.4/src/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/lua-5.4.4/src/*.c"
)

list(REMOVE_ITEM LUA_SOURCES "${CMAKE_CURRENT_LIST_DIR}/lua-5.4.4/src/lua.c")
list(REMOVE_ITEM LUA_SOURCES "${CMAKE_CURRENT_LIST_DIR}/lua-5.4.4/src/luac.c")

add_library(lua STATIC
    ${LUA_SOURCES})

target_include_directories(lua PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/lua-5.4.4/src")

add_library(Zstd SHARED IMPORTED GLOBAL)
set_target_properties(Zstd PROPERTIES
    IMPORTED_IMPLIB "${ZSTD_LIBRARIES}"
    IMPORTED_LOCATION "${ZSTD_BINARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIR}")