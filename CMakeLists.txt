cmake_minimum_required(VERSION 3.20)

project(P001_Basic_OpenGL_Cpp)

set(CMAKE_CXX_STANDARD 17)

add_executable(P001_Basic_OpenGL_Cpp
    srcmain.cpp
    srcTextRenderer.cpp
    srcAudioManager.cpp
    srcglad.c
)

target_include_directories(P001_Basic_OpenGL_Cpp PRIVATE
    librariesinclude
    src
)

# Link libraries here according to your platform.
#
# Example
#
# target_link_libraries(P001_Basic_OpenGL_Cpp
#     glfw
#     freetype
#     OpenAL
# )