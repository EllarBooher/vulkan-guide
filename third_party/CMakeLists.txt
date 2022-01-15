find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)
add_library(tinyobjloader STATIC)
add_library(fmt_lib STATIC)
add_library(imgui STATIC)

add_library(sdl2 INTERFACE)

#header only libraries
add_library(glm INTERFACE)
add_library(vma INTERFACE)
add_library(stb_image INTERFACE)

#vkb
target_sources(vkbootstrap PRIVATE 
    "vkbootstrap/VkBootstrap.h"
    "vkbootstrap/VkBootstrap.cpp")
target_include_directories(vkbootstrap PUBLIC "vkbootstrap")
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)

#vma
target_include_directories(vma INTERFACE "vma")

#glm
target_include_directories(glm INTERFACE "glm")

#stbi
target_include_directories(stb_image INTERFACE "stb_image")

#tol
target_include_directories(tinyobjloader PUBLIC "tinyobjloader")
target_sources(tinyobjloader PRIVATE 
    "tinyobjloader/tiny_obj_loader.h"
    "tinyobjloader/tiny_obj_loader.cc")

#sdl
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(SDL2 REQUIRED)

message("SDL library directories: " ${SDL2_LIBRARIES})
target_include_directories(sdl2 INTERFACE ${SDL2_INCLUDE_DIRS})
target_link_libraries(sdl2 INTERFACE ${SDL2_LIBRARIES})

#imgui
target_include_directories(imgui PUBLIC "imgui" "imgui/backends")
target_sources(imgui PRIVATE 
    "imgui/imgui.h"
    "imgui/imgui.cpp"
    "imgui/imgui_demo.cpp"
    "imgui/imgui_draw.cpp"
    "imgui/imgui_widgets.cpp"
    "imgui/imgui_tables.cpp"
    "imgui/backends/imgui_impl_vulkan.cpp"
    "imgui/backends/imgui_impl_sdl.cpp")
target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)

#fmt
target_include_directories(fmt_lib PUBLIC "fmt_lib")
target_sources(fmt_lib PRIVATE "fmt_lib/fmt/format.cc" "fmt_lib/fmt/os.cc")
