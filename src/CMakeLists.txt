
# Add source to this project's executable.
add_executable(vulkan_guide
    main.cpp
    vk_engine.cpp
    vk_engine.h
    vk_types.h
    vk_mesh.h
    vk_mesh.cpp
    vk_initializers.cpp
    vk_initializers.h
    input_manager.cpp
    input_manager.h 
    "vk_textures.h" 
    "vk_textures.cpp" 
    "player_camera.h" 
    "player_camera.cpp" 
    "logger.h" 
    "vk_shaders.cpp" 
    "vk_shaders.h" 
    "vk_descriptors.h" 
    "vk_descriptors.cpp" 
    "vk_pipelines.h" 
    "vk_pipelines.cpp" 
    "vk_buffers.h" 
    "vk_frame.h" "vk_buffers.cpp")

set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2 fmt_lib)

add_dependencies(vulkan_guide Shaders)