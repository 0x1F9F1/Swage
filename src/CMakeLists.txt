include_directories(${CMAKE_CURRENT_LIST_DIR})

link_libraries(SDL2 fmt imgui zlibstatic wolfcrypt Zstd)

SET(SWAGE_PCH "${CMAKE_CURRENT_LIST_DIR}/swpch.h")

add_compile_options($<$<CONFIG:MinSizeRel,Release>:/GR->)

add_library(Swage STATIC
    swage.h
    swage.cpp

    app.h
    app.cpp
)

target_link_libraries(Swage SDL2main)

target_precompile_headers(Swage PRIVATE ${SWAGE_PCH})

set_target_properties(Swage PROPERTIES
    SWAGE_COMPONENTS "")

macro(swage_add_component name)
    if (NOT ${name} MATCHES "^Sw")
        message(FATAL_ERROR "Invalid Swage Component '${name}' (should begin with 'Sw')")
    endif()
    add_library(${name} OBJECT ${ARGN})
    set_property(TARGET Swage APPEND PROPERTY SWAGE_COMPONENTS ${name})
    target_precompile_headers(${name} PRIVATE ${SWAGE_PCH})
endmacro(swage_add_component)

add_subdirectory(asset)
add_subdirectory(core)
add_subdirectory(crypto)

add_subdirectory(games)

get_target_property(SWAGE_COMPONENTS Swage SWAGE_COMPONENTS)

target_link_libraries(Swage ${SWAGE_COMPONENTS})

add_subdirectory(explorer)
target_precompile_headers(ArchiveExplorer PRIVATE ${SWAGE_PCH})