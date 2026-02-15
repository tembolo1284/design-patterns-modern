cmake_minimum_required(VERSION 3.20)
project(cpp_design_patterns LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ Strategy
add_executable(strategy_classic strategy/classic/main.cpp)
add_executable(strategy_modern strategy/modern/main.cpp)

# C++ Visitor
add_executable(visitor_classic visitor/classic/main.cpp)
add_executable(visitor_modern visitor/modern/main.cpp)

# C++ Command
add_executable(command_classic command/classic/main.cpp)
add_executable(command_modern command/modern/main.cpp)

# Pure C
add_executable(c_strategy c/strategy.c)
add_executable(c_visitor c/visitor.c)
add_executable(c_command c/command.c)
target_link_libraries(c_strategy m)
target_link_libraries(c_visitor m)
target_link_libraries(c_command m)
