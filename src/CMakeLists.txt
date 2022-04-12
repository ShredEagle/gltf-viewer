add_subdirectory(apps/gltf-viewer/gltf-viewer)

option (BUILD_tests "Build 'tests' application" true)
if(BUILD_tests)
    add_subdirectory(apps/gltf-viewer_tests)
endif()
