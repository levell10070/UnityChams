cmake_minimum_required(VERSION 3.12)
project(ShaderDumper)

add_library(ShaderDumper SHARED dllmain.cpp "Shader.cpp" "Texture.cpp" "Vectors.h" "FrameBuffer.cpp")

find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	# For Visual C++
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
else()
	# For GCC and Clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()


include (FetchContent)

FetchContent_Declare(
	minhook
	GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
)

FetchContent_MakeAvailable(
	minhook
)

set (GLEW_DIR Dependencies/glew-2.1.0)

add_subdirectory(${GLEW_DIR}/build/cmake)

include_directories(${GLEW_DIR}/include)

target_link_libraries(ShaderDumper glew_s OpenGL::GL minhook)


