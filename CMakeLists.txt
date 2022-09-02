cmake_minimum_required(VERSION 3.1)

set(PROJECT troglodite)
project(${PROJECT})
set(EXEC ${PROJECT})

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O1 -DDEBUG -lvulkan")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -lvulkan")

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${EXEC} ${SOURCES})
target_include_directories(${PROJECT} PRIVATE src/)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(Vulkan REQUIRED FATAL_ERROR)
target_link_libraries(${PROJECT} Vulkan::Vulkan)

add_subdirectory(lib/glfw)
target_link_libraries(${PROJECT} glfw)

add_subdirectory(lib/vk-bootstrap)
target_link_libraries(${PROJECT} vk-bootstrap::vk-bootstrap)

add_subdirectory(lib/glm)
target_link_libraries(${PROJECT} glm::glm)

add_definitions(-DGLFW_INCLUDE_NONE)
