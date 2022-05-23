set(TARGET_NAME gltf-viewer)

set(${TARGET_NAME}_HEADERS
    Camera.h
    DataLayout.h
    DebugDrawer.h
    GltfAnimation.h
    GltfRendering.h
    ImguiUi.h
    LoadBuffer.h
    Logging.h
    Mesh.h
    Polar.h
    Scene.h
    Shaders.h
    ShadersPbr.h
    ShadersPbr_learnopengl.h
    SkeletalAnimation.h
    Url.h
    UserOptions.h
)

set(${TARGET_NAME}_SOURCES
    Camera.cpp
    DebugDrawer.cpp
    GltfAnimation.cpp
    GltfRendering.cpp
    ImguiUi.cpp
    LoadBuffer.cpp
    Logging.cpp
    main.cpp
    Mesh.cpp
    Scene.cpp
    SkeletalAnimation.cpp
)

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}
             FILES ${${TARGET_NAME}_HEADERS} ${${TARGET_NAME}_SOURCES}
)

set(${TARGET_NAME}_IMGUI_BACKENDS
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_glfw.cpp
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_opengl3.cpp
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_glfw.h
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_opengl3.h
    ${PROJECT_BINARY_DIR}/conan_imports/imgui_backends/imgui_impl_opengl3_loader.h
)

add_executable(${TARGET_NAME}
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
    ${${TARGET_NAME}_IMGUI_BACKENDS}
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        ${PROJECT_BINARY_DIR}/conan_imports
)

##
## Dependencies
##
find_package(Graphics CONFIG REQUIRED COMPONENTS arte graphics)
find_package(Handy CONFIG REQUIRED COMPONENTS resource)

find_package(Boost REQUIRED COMPONENTS program_options)
find_package(imgui REQUIRED)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ad::arte
        ad::graphics
        ad::resource

        Boost::program_options
        imgui::imgui
)
