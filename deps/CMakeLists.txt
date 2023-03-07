set(OpenGL_GL_PREFERENCE GLVND)
add_subdirectory(raylib)

add_library(acutest INTERFACE)
target_include_directories(acutest INTERFACE acutest)

add_library(simde INTERFACE)
target_include_directories(simde INTERFACE simde)
