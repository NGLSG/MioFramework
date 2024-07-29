cmake_minimum_required(VERSION 3.28)
set(name mlib)

project(${name})

add_subdirectory(sol2)
add_subdirectory(yaml-cpp)
add_subdirectory(EURL)
add_subdirectory(RyoCompression)
add_subdirectory(glfw)

find_package(OpenGL REQUIRED)

file(GLOB isrc
        ImGUI/*.cpp
        ImGUI/backends/imgui_impl_glfw.*
        ImGUI/backends/imgui_impl_opengl3.*
        ImGUI/backends/imgui_impl_opengl3_loader.h

)
add_library(ImGui STATIC ${isrc})
target_link_libraries(ImGui PUBLIC glfw ${OPENGL_gl_LIBRARY})
target_include_directories(ImGui PUBLIC ImGUI)