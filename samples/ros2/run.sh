#!/bin/bash

# Set colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   vsemi_limu ROS2 Application Runner  ${NC}"
echo -e "${GREEN}========================================${NC}"

# Check if ROS2 is sourced
if [ -z "$ROS_DISTRO" ]; then
    echo -e "${YELLOW}ROS2 not detected. Sourcing ROS2...${NC}"
    source /opt/ros/humble/setup.bash
fi

# Check if workspace is built
if [ ! -f "install/setup.bash" ]; then
    echo -e "${YELLOW}Workspace not built. Building vsemi_limu package...${NC}"
    colcon build --packages-select vsemi_limu
    if [ $? -ne 0 ]; then
        echo -e "${RED}Build failed! Please check the errors above.${NC}"
        exit 1
    fi
fi

# Source the workspace
echo -e "${GREEN}Sourcing workspace...${NC}"
source install/setup.bash

# Check if data files exist
DATA_PATH="/home/vsemi/data/test"
if [ ! -f "$DATA_PATH/test.pcd" ] || [ ! -f "$DATA_PATH/test.jpg" ]; then
    echo -e "${RED}Error: Data files not found in $DATA_PATH${NC}"
    exit 1
fi

echo -e "${GREEN}Data files found successfully!${NC}"

# Parse command line arguments
PUBLISH_RATE=1.0
USE_RVIZ="true"
RENDER_MODE="software"  # software or hardware

while [[ $# -gt 0 ]]; do
    case $1 in
        --rate)
            PUBLISH_RATE="$2"
            shift 2
            ;;
        --no-rviz)
            USE_RVIZ="false"
            shift
            ;;
        --hardware)
            RENDER_MODE="hardware"
            shift
            ;;
        --help)
            echo "Usage: ./run.sh [options]"
            echo "Options:"
            echo "  --rate <hz>     Set publish rate (default: 1.0 Hz)"
            echo "  --no-rviz       Run without RViz visualization"
            echo "  --hardware      Try hardware acceleration (default: software)"
            echo "  --help          Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}Starting vsemi_limu node with:${NC}"
echo -e "  ${YELLOW}Publish rate:${NC} $PUBLISH_RATE Hz"
echo -e "  ${YELLOW}RViz enabled:${NC} $USE_RVIZ"
echo -e "  ${YELLOW}Render mode:${NC} $RENDER_MODE"

# Set environment variables based on render mode
if [ "$RENDER_MODE" = "software" ]; then
    echo -e "${YELLOW}Using software rendering (may be slower but more reliable)${NC}"
    export LIBGL_ALWAYS_SOFTWARE=1
    export LIBGL_ALWAYS_INDIRECT=0
    export QT_QUICK_BACKEND=software
    export LIBGL_DRI3_DISABLE=1
else
    echo -e "${YELLOW}Attempting hardware acceleration${NC}"
    # Unset software rendering variables
    unset LIBGL_ALWAYS_SOFTWARE
    unset LIBGL_ALWAYS_INDIRECT
    unset QT_QUICK_BACKEND
fi

# Run the launch file
echo -e "${GREEN}Launching application...${NC}"
ros2 launch vsemi_limu vsemi_limu_launch.py \
    publish_rate:=$PUBLISH_RATE \
    use_rviz:=$USE_RVIZ
