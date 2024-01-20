cmake_minimum_required(VERSION 3.16)
#set(CMAKE_VERBOSE_MAKEFILE ON)
project (timer_pulse)
set(Executable_Name "tpulse")

add_compile_options(-Wall)
add_compile_options(-Wextra)
add_compile_options(-D_POSIX_C_SOURCE=199309L) #this fixes CLOCK_MONOTONIC undefined
# set(CMAKE_CXX_FLAGS "-std=gnu99")
# set(CMAKE_CXX_FLAGS "-D_DEFAULT_SOURCE -std=c99")  
set_property(GLOBAL PROPERTY C_STANDARD 99)

find_package(BLAS REQUIRED) # Basic Linear Algebra Subprograms
find_package(GSL REQUIRED)
include_directories("/usr/local/include/gsl")

# file(GLOB SRC_FILES CONFIGURE_DEPENDS "*.c")
add_executable(${Executable_Name} main.c)

set(TARGET_NAME ${Executable_Name})

target_link_libraries(${Executable_Name} rt gpiod gsl gslcblas)

set_target_properties(${Executable_Name} PROPERTIES OUTPUT_NAME "${Executable_Name}.bin")
# set_target_properties(${Executable_Name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "/home/pi/boomer/staged")
