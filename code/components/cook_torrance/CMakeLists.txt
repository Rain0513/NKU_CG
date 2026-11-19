cmake_minimum_required(VERSION 3.18)

set(MY_COMPONENT_NAME "CookTorrance")

# Reuse the triangle/BVH/shadow pipeline; the adapter supplies its own BRDF.
add_library(${MY_COMPONENT_NAME} SHARED
    "./include/CookTorrance.hpp"
    "./src/Adapter.cpp"
    "./src/CookTorrance.cpp"
    "../phong_shading/src/PhongRenderer.cpp"
)
target_include_directories(${MY_COMPONENT_NAME} PRIVATE
    "./include"
    "../phong_shading/include"
)
target_link_libraries(${MY_COMPONENT_NAME} NRServer)
add_dependencies(NRenderer ${MY_COMPONENT_NAME})
