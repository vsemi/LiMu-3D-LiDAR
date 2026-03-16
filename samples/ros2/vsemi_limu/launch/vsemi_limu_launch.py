import os
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, SetEnvironmentVariable
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    package_name = 'vsemi_limu'
    
    # Get package path
    pkg_share = get_package_share_directory(package_name)
    
    # Default config file path
    default_rviz_config = os.path.join(pkg_share, 'config', 'vsemi_limu.rviz')
    
    return LaunchDescription([
        # Set multiple environment variables for software rendering
        # SetEnvironmentVariable(name='LIBGL_ALWAYS_SOFTWARE', value='1'),
        # SetEnvironmentVariable(name='LIBGL_ALWAYS_INDIRECT', value='0'),
        # SetEnvironmentVariable(name='QT_QUICK_BACKEND', value='software'),
        # SetEnvironmentVariable(name='LIBGL_DRI3_DISABLE', value='1'),
        
        DeclareLaunchArgument(
            'data_path',
            default_value='/home/vsemi/data/test',
            description='Path to data directory'
        ),
        DeclareLaunchArgument(
            'pcd_file',
            default_value='test.pcd',
            description='PCD file name'
        ),
        DeclareLaunchArgument(
            'jpg_file',
            default_value='test.jpg',
            description='JPG file name'
        ),
        DeclareLaunchArgument(
            'publish_rate',
            default_value='1.0',
            description='Publish rate in Hz'
        ),
        DeclareLaunchArgument(
            'rviz_config',
            default_value=default_rviz_config,
            description='RViz config file path'
        ),
        DeclareLaunchArgument(
            'use_rviz',
            default_value='true',
            description='Launch RViz2 automatically'
        ),
        
        # Main node
        Node(
            package=package_name,
            executable='vsemi_limu_node',
            name='vsemi_limu_node',
            output='screen',
            parameters=[{
                'data_path': LaunchConfiguration('data_path'),
                'pcd_file': LaunchConfiguration('pcd_file'),
                'jpg_file': LaunchConfiguration('jpg_file'),
                'publish_rate': LaunchConfiguration('publish_rate')
            }]
        ),

        # Static transform publisher
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            arguments=['0', '0', '0', '0', '0', '0', 'map', 'vsemi_limu_frame'],
            name='static_broadcaster'
        ),
        
        # RViz2 node with additional environment variables
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', LaunchConfiguration('rviz_config')],
            condition=LaunchConfigurationEquals('use_rviz', 'true'),
            additional_env={
                # 'LIBGL_ALWAYS_SOFTWARE': '1',
                # 'LIBGL_ALWAYS_INDIRECT': '0',
                # 'QT_QUICK_BACKEND': 'software'
            }
        )
    ])

class LaunchConfigurationEquals:
    def __init__(self, variable_name, value):
        self.variable_name = variable_name
        self.value = value
    
    def evaluate(self, context):
        return context.launch_configurations.get(self.variable_name) == self.value
