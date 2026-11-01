cmake_minimum_required(VERSION 3.8)
project(deliverbot_cloud)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(deliverbot_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

add_executable(thingsboard src/thingsboard_node.cpp)
target_include_directories(thingsboard PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
target_link_libraries(thingsboard ${MOSQUITTO_LIBRARIES})
ament_target_dependencies(thingsboard rclcpp geometry_msgs deliverbot_msgs std_msgs)

install(TARGETS 
  thingsboard

  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
