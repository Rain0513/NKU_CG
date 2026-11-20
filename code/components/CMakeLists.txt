cmake_minimum_required(VERSION 3.18)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/components)
if (MSVC)
	add_compile_options(/utf-8)
endif()

# add your component directory
add_subdirectory("./example")
add_subdirectory("./simple_path_tracing")
add_subdirectory("./ray_cast")
add_subdirectory("./phong_shading")
add_subdirectory("./cook_torrance")
add_subdirectory("./ray_tracing")
