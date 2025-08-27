cmake_minimum_required(VERSION 3.14)

project(db)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # generate compile_commands.json
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

file(GLOB DB_SOURCES "src/*.cpp")
list(REMOVE_ITEM DB_SOURCES "src/main.cpp")

add_executable(${PROJECT_NAME} ${DB_SOURCES} "src/repl/main.cpp")

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

enable_testing()

add_subdirectory(tests)
