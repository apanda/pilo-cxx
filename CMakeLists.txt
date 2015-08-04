cmake_minimum_required (VERSION 2.8) 
project (PCPP)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PCPP_SOURCE_DIR}/cmake_modules")

message("CMake module path ${CMAKE_MODULE_PATH}")
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_CXX_FLAGS "-stdlib=libc++")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -std=c++11")

find_package(Boost 1.54 COMPONENTS program_options)
if (Boost_FOUND)
    message("boost found ${Boost_INCLUDE_DIRS} ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
else ()
    message(FATAL_ERROR "Boost not found")
endif(Boost_FOUND)
find_package(YamlCpp REQUIRED)

if (YAMLCPP_FOUND)
    message("YAML found ${YAMLCPP_LIBRARY} ${YAMLCPP_INCLUDE_DIR}")
    include_directories(${YAMLCPP_INCLUDE_DIR})
else ()
    message(FATAL_ERROR "yaml-cpp not found")
endif(YAMLCPP_FOUND)

include_directories(${PCPP_SOURCE_DIR}/include)

file(GLOB pcpp_sources . src/*.cc)
add_executable(pilo ${pcpp_sources})
target_link_libraries(pilo ${Boost_LIBRARIES})
target_link_libraries(pilo ${YAMLCPP_LIBRARY})
