cmake_minimum_required(VERSION 3.8)
project(deliverbot_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(deliverbot_msgs REQUIRED)

add_executable(nav2_clint src/nav2_action_clinet.cpp)
ament_target_dependencies(nav2_clint rclcpp nav2_msgs tf2 tf2_geometry_msgs geometry_msgs std_msgs rclcpp_action deliverbot_msgs)

add_executable(goal_maker src/goal_maker.cpp)
ament_target_dependencies(goal_maker rclcpp deliverbot_msgs geometry_msgs tf2 tf2_geometry_msgs)

add_executable(order_control src/order_controller.cpp)
ament_target_dependencies(order_control rclcpp rclcpp_action deliverbot_msgs geometry_msgs std_msgs)

add_executable(door_controller src/door_controller.cpp)
ament_target_dependencies(door_controller rclcpp  deliverbot_msgs geometry_msgs std_msgs)

add_executable(order_request src/order_request.cpp)
ament_target_dependencies(order_request rclcpp rclcpp_action deliverbot_msgs)

install(TARGETS 
  nav2_clint
  goal_maker
  order_control
  door_controller
  order_request
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
