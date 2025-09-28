cmake_minimum_required(VERSION 3.16)

function(create_executable EXE_NAME SOURCE_DIR)
    file (GLOB_RECURSE EXE_SRC "${SOURCE_DIR}/*.cpp")
    file (GLOB_RECURSE EXE_HDR "${SOURCE_DIR}/*.h")
    add_executable(${EXE_NAME} ${EXE_SRC} ${EXE_HDR})
    target_include_directories(${EXE_NAME} PRIVATE "${SOURCE_DIR}")
    target_include_directories(${EXE_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/includes")
    target_link_libraries(${EXE_NAME} PRIVATE nlohmann_json)
    install(TARGETS ${EXE_NAME})
endfunction()

set(PROJECT_NAME "Transmission_protocols")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE "Release")
set(WARNINGS_AS_ERRORS OFF)

set(NLOHMANN_JSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json")
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${NLOHMANN_JSON_DIR}/single_include")
target_compile_features(nlohmann_json INTERFACE cxx_std_11)

# ==================================================
# Конец конфигурационного блока
# ==================================================

project(${PROJECT_NAME} LANGUAGES CXX)

add_subdirectory(UDP)