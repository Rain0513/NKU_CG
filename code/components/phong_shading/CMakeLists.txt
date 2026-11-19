cmake_minimum_required(VERSION 3.18)

set(MY_COMPONENT_NAME "PhongShading")

file(GLOB_RECURSE COMP_HEADER_FILES "./include/*.h" "./include/*.hpp")
source_group("Header Files" FILES ${COMP_HEADER_FILES})
set(COMP_SOURCE_FILES
    "./src/Adapter.cpp"
    "./src/PerPixelAdapter.cpp"
    "./src/PhongRenderer.cpp"
)
add_library(${MY_COMPONENT_NAME} SHARED "${COMP_SOURCE_FILES}" "${COMP_HEADER_FILES}")
target_link_libraries(${MY_COMPONENT_NAME} NRServer)

# Building the application's launch target must also rebuild its renderer DLL.
add_dependencies(NRenderer ${MY_COMPONENT_NAME})

include_directories("./include")
