cmake_minimum_required(VERSION 3.6)

#find_package(GLFW3 CONFIG REQUIRED)
#include_directories(../glfw)
#add_subdirectory(../glfw glfw)


if(CMAKE_VERSION VERSION_GREATER 3.12)
        add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD=1)
else()
        add_definitions("-DIMGUI_IMPL_OPENGL_LOADER_GLAD=1")
endif()

add_library(
        imgui STATIC
	imconfig.h
	imgui.cpp
	imgui_demo.cpp
	imgui_draw.cpp
	imgui.h
	imgui_impl_glfw.cpp
	imgui_impl_glfw.h
	imgui_impl_opengl3.cpp
	imgui_impl_opengl3.h
	imgui_impl_opengl3_loader.h
	imgui_internal.h
	imgui_tables.cpp
	imgui_widgets.cpp
	imstb_rectpack.h
	imstb_textedit.h
	imstb_truetype.h
        )

target_link_libraries(imgui PRIVATE glfw glad)
