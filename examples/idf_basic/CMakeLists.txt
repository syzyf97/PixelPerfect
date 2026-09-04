cmake_minimum_required(VERSION 3.16)

# Make the PixelPerfect repo root (two levels up) visible as a component.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(idf_basic)
