include_directories("${PROJECT_SOURCE_DIR}/src")

add_executable(benchmarks benchmarks.cpp)
target_link_libraries(benchmarks
    game
    game-solver
)
