####
# Builds the PiCanTest project: a deployment that exercises fprime-socketcan
# between two Raspberry Pi 5s with Waveshare 2-CH CAN HATs.
####

cmake_minimum_required(VERSION 3.24.2)
project(PiCanTest C CXX)

###
# F' Core Setup
# This includes all of the F prime core components, and imports the make-system.
###
include("${CMAKE_CURRENT_LIST_DIR}/lib/fprime/cmake/FPrime.cmake")
# NOTE: register custom targets between these two lines
fprime_setup_included_code()

# This includes project-wide objects
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/PiCanTest/")
