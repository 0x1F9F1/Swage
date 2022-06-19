add_executable(ArchiveExplorer
    explorer.cpp

    gui.cpp
    gui.h

    imgui_impl_sdlrenderer.cpp
    imgui_impl_sdlrenderer.h

    imgui_impl_sdl.cpp
    imgui_impl_sdl.h

    resource.h
    explorer.rc
)

target_link_libraries(ArchiveExplorer Swage yaml-cpp lua)

add_custom_command(TARGET ArchiveExplorer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL2> "${CMAKE_CURRENT_LIST_DIR}/README.txt"
    $<TARGET_FILE_DIR:ArchiveExplorer>)
