cmake_minimum_required(VERSION 3.10)

project(stm32_i2c_v2_timing_calc)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_executable(stm32_i2c_v2_timing_calc src/main.cpp)
target_include_directories(stm32_i2c_v2_timing_calc PRIVATE ./third_party/argparse/include)
install(TARGETS stm32_i2c_v2_timing_calc)
