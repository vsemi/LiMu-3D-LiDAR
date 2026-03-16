#!/bin/bash

echo -e "${GREEN}Rebuilding vsemi_limu package...${NC}"

# Source ROS2
source /opt/ros/humble/setup.bash

# Clean build (optional, remove if you want faster rebuilds)
rm -rf build/vsemi_limu install/vsemi_limu

# Build the package
colcon build --packages-select vsemi_limu --symlink-install

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    source install/setup.bash
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
