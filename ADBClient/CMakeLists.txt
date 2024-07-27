cmake_minimum_required(VERSION 3.28)
set(name adbc)
project(${name})
file(GLOB src
        *.cpp
)
file(GLOB header
        *.hpp
        *.h
)


add_library(adbc-static STATIC ${src} ${header})

add_library(adbc ALIAS adbc-static)