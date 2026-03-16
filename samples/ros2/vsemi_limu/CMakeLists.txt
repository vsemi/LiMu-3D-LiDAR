cmake_minimum_required(VERSION 3.8)
project(vsemi_limu)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(PCL REQUIRED)
find_package(OpenCV REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package (Threads)

include_directories(include)
include_directories(${PCL_INCLUDE_DIRS})
add_definitions(${PCL_DEFINITIONS})

include_directories( 
  ../../../driver/include
  ../../../driver/src
  ${Boost_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
  ${catkin_INCLUDE_DIRS}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
  ${PCL_LIBRARY_DIRS}
  {CMAKE_THREAD_LIBS_INIT}
)

file(GLOB _SRC
  "../../../driver/src/*.cpp"
    "src/*.cpp"
)

# Create executable
add_executable(vsemi_limu_node
  ${_SRC}
)

# Link libraries
ament_target_dependencies(vsemi_limu_node
  rclcpp
  sensor_msgs
  std_msgs
  cv_bridge
  pcl_conversions
)

target_link_libraries(vsemi_limu_node
  ${PCL_LIBRARIES}
  ${OpenCV_LIBRARIES}
  ${Boost_LIBRARIES}
)

# Install executable
install(TARGETS
  vsemi_limu_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in the git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

# Install config files
install(
  DIRECTORY config
  DESTINATION share/${PROJECT_NAME}/
)

# Install launch files
install(
  DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()