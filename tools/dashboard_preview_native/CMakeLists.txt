cmake_minimum_required(VERSION 3.20)

project(dashboard_preview_native LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

add_executable(dashboard_preview_native
  src/main.mm
  ../../src/dash_35/app/dashboard_scene.cpp
  ../../src/dash_35/app/dashboard_state.cpp
  ../../src/dash_35/app/vehicle_profiles/vehicle_profile.cpp
)

target_include_directories(dashboard_preview_native PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/dash_35/app
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/dash_35/app/vehicle_profiles
)

target_link_libraries(dashboard_preview_native PRIVATE
  "-framework AppKit"
  "-framework Foundation"
)