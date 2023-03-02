# raylib
set(OpenGL_GL_PREFERENCE GLVND)
add_subdirectory(raylib)

# acutest
add_library(acutest INTERFACE)
target_include_directories(acutest INTERFACE ${CMAKE_CURRENT_LIST_DIR})
