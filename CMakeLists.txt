cmake_minimum_required(VERSION 3.28)
project(MioFramework)


set(CMAKE_CXX_STANDARD 20)

find_package(CURL REQUIRED)
find_package(Lua REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)


add_subdirectory(ADBClient)
add_subdirectory(MUI)
add_subdirectory(Library)
file(GLOB src
        src/*.c*
)
file(GLOB header
        src/*.h*
)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj /utf-8")
endif ()
add_definitions(-DBORINGSSL_IMPLEMENTATION)
add_definitions(-DNOCRYPT)
include_directories(${OpenCV_INCLUDE_DIRS} ADBClient)
add_executable(MioFramework ${src} ${header})
target_include_directories(MioFramework PUBLIC ${LUA_INCLUDE_DIR})
target_link_libraries(MioFramework PUBLIC eurl adbc MUI ${OpenCV_LIBS} ${LUA_LIBRARIES} sol2::sol2 )
