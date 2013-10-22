project(alloaudiocontrol)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

# Jack dependency
target_link_libraries(${PROJECT_NAME} jack)
include_directories("/usr/include")

# Liblo dependency
target_link_libraries(${PROJECT_NAME} lo)
include_directories("/usr/include")

#fftw3 dependency
target_link_libraries(${PROJECT_NAME} fftw3)
include_directories("/usr/include")

#pthreads dependency
find_package(Threads)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

