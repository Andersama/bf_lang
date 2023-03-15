# CMakeList.txt : CMake project for bf_lang, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (bf_lang "bf_lang.cpp" "bf_lang.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET bf_lang PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
