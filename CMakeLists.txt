cmake_minimum_required(VERSION 3.16)
project(CacheSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_library(cache_sim_lib
    src/CacheLine.cpp
    src/CacheSet.cpp
    src/Cache.cpp
    src/IReplacementPolicy.cpp
    src/TraceReader.cpp
)

target_include_directories(cache_sim_lib PUBLIC include)


add_executable(cache_sim src/main.cpp)
target_link_libraries(cache_sim PRIVATE cache_sim_lib)

enable_testing()
add_subdirectory(tests)
