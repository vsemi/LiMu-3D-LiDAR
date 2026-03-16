cmake_minimum_required(VERSION 3.5.0)

PROJECT(limu_driver)

set(CMAKE_CXX_STANDARD 11)

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

message( STATUS "Architecture: ${ARCHITECTURE}" )

set(CMAKE_CXX_FLAGS "-fPIC")

set(CMAKE_INSTALL_PREFIX "../")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

find_package (Threads)

find_package(Boost COMPONENTS thread system REQUIRED)

message("Boost version: ${Boost_VERSION}")

file(GLOB lib_INCLUDE_DIR
    "include"
    "src"
)

file(GLOB lib_SRC
    "src/*.cpp"
)

include_directories( ${Boost_INCLUDE_DIRS} ${lib_INCLUDE_DIR} )

link_directories( ${Boost_LIBRARY_DIRS} )

ADD_LIBRARY( limu_driver SHARED ###SHARED ###STATIC
             ${lib_SRC} )

TARGET_LINK_LIBRARIES(limu_driver ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install (TARGETS limu_driver DESTINATION "lib/${ARCHITECTURE}")


