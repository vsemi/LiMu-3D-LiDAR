#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/header.hpp>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include <filesystem>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>  // Added for std::atomic
#include <functional>  // Added for std::function
#include <iostream>  // Added for std::cout/cerr
#include <thread>

#include <pcl/common/transforms.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/filters/filter.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/filters/conditional_removal.h>
#include <pcl/filters/project_inliers.h>

#include <pcl/search/organized.h>
#include <pcl/search/kdtree.h>

#include <pcl/features/moment_of_inertia_estimation.h>

#include <pcl/surface/concave_hull.h>

#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>

#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>  // Fixed: was video/video.hpp

#include "tof.hpp"
#include "frame.hpp"

using namespace std::chrono_literals;

std::atomic<bool> g_ros_running{true};

int lensType = 0;  //0- wide field, 1- standard field, 2 - narrow field
int frequencyModulation = 2;
int imageType = 1; //image and aquisition type: 0 - grayscale, 1 - distance, 2 - distance_amplitude
int hdr_mode = 0;
int int0 = 800, int1 = 200, int2 = 50, intGr = 10000;
int minAmplitude = 60;
int lensCenterOffsetX = 0;
int lensCenterOffsetY = 0;

int roi_leftX = 0;
int roi_topY = 0;
int roi_rightX = 319;
int roi_bottomY = 239;

class LimuLidar {
public:
    LimuLidar(int channel, const char* host, const char* rgb_camera_path) : m_channel(channel), m_host(host), m_rgb_camera_path(rgb_camera_path) {
        m_tof = nullptr;  // Initialize pointer
        m_videoCapture = nullptr;  // Initialize pointer
    }

    bool start() {
        std::cout << "Starting ToF: " << m_channel << std::endl;
        m_tof = ToF::tof320(m_host, "50660");

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        int wait_seconds = 0;
        while (!m_tof->isConnected()) {            
            if (wait_seconds > 5) {
                std::cout << "ERROR: First camera failed to connect!" << std::endl;
                shutdown();
                return false;
            }
            wait_seconds ++;
            std::this_thread::sleep_for(std::chrono::seconds(1));  // Added sleep to avoid busy wait
        }
        
        setParameters();

        startStreaming();
        
        m_videoCapture = new cv::VideoCapture();
        std::cerr << "Starting RGB camera: " << m_rgb_camera_path << std::endl;
        try {
            m_rgb_available = m_videoCapture->open(m_rgb_camera_path);
            if (m_rgb_available)
            {
                m_videoCapture->set(cv::CAP_PROP_FRAME_WIDTH, 640);
                m_videoCapture->set(cv::CAP_PROP_FRAME_HEIGHT, 480);

                std::cerr << "RGB camera started." << std::endl;

                // Fixed: pass 'this' to thread function
                std::thread th_start_RGB(&LimuLidar::start_RGB_loop, this);
                th_start_RGB.detach();
            } else
            {
                std::cerr << "Warning: RGB camera is not available! " << std::endl;
            }
        } catch (...)
        {
            std::cerr << "Error: unexpected error when initializing RGB camera!" << std::endl;
        }

        std::cout << "ToF camera initialized successfully" << std::endl;
        return true;  // Added return value
    }

    void subscribeToF(std::function<void (std::shared_ptr<Frame>)> func) {
        m_tof->subscribeFrame(func);
    }

    void setParameters()
    {
        std::cout << "set parameters ..." << std::endl;

        m_tof->stopStream();

        uint8_t modIndex;
        if(frequencyModulation == 0) modIndex = 1;
        else if(frequencyModulation == 1)  modIndex = 0;
        else    modIndex = frequencyModulation;

        m_tof->setModulation(modIndex, m_channel);
        m_tof->setMinAmplitude(minAmplitude);
        m_tof->setIntegrationTime(int0, int1, int2, intGr);
        m_tof->setHDRMode((uint8_t)hdr_mode);
        m_tof->setRoi(roi_leftX, roi_topY, roi_rightX, roi_bottomY);
        m_tof->setLensType(lensType);
        m_tof->setLensCenter(lensCenterOffsetX, lensCenterOffsetY);
        m_tof->setFilter(0, 0, 0, 0, 0, 0, 0, 0, 0);

        std::cout << "lens_type: " << lensType << std::endl;
        std::cout << "frequency_modulation: " << frequencyModulation << std::endl;
        std::cout << "channel: " << m_channel << std::endl;
        std::cout << "image_type: " << imageType << std::endl;
        std::cout << "hdr_mode: " << hdr_mode << std::endl;
        std::cout << "integration_time0: " << int0 << std::endl;
        std::cout << "integration_time1: " << int1 << std::endl;
        std::cout << "integration_time2: " << int2 << std::endl;
        std::cout << "min_amplitude: " << minAmplitude << std::endl;
        std::cout << "integration_time_gray: " << intGr << std::endl;

        std::cout << "Start camera: " << m_channel << std::endl;
        startStreaming();
    }
    
    void startStreaming()
    {
        switch(imageType) {
        case 0:  // Assuming Frame::DISTANCE equals 0
            std::cout << "Start streaming distance: " << m_channel << std::endl;
            m_tof->streamDistance();
            std::cout << "      started: " << m_channel << std::endl;
            break;
        case 1:  // Assuming Frame::AMPLITUDE equals 1
            std::cout << "Start streaming distance-amplitude: " << m_channel << std::endl;
            m_tof->streamDistanceAmplitude();
            std::cout << "      started: " << m_channel << std::endl;
            break;
        default:
            break;
        }
    }

    void start_RGB_loop()
    {
        cv::Mat frame;
        if (m_rgb_available)
        {
            while(g_ros_running)
            {
                try
                {
                    if (m_videoCapture->read(frame)) {
                        if ((! frame.empty()) && frame.rows > 0 && frame.cols > 0)
                        {
                            {
                                std::lock_guard<std::mutex> lock(rgb_image_mutex);
                                rgb_image = frame.clone();
                            }
                        }
                    }
                } catch (...) {
                    std::cerr << "Something wrong with RGB camera!" << std::endl;
                }
                
                // Small sleep to prevent busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    void shutdown() {
        if (m_tof) {
            m_tof->stopStream();
            m_tof->shutdown();
        }

        if (m_rgb_available && m_videoCapture)
        {
            m_videoCapture->release();
        }

        delete m_tof;
        delete m_videoCapture;
        m_videoCapture = nullptr;
    }

    bool is_rgb_available() {
        return m_rgb_available;
    }

    cv::Mat current_rgb_image() {
        std::lock_guard<std::mutex> lock(rgb_image_mutex);
        return rgb_image.clone();  // Return a copy to avoid race conditions
    }

private:
    int m_channel;
    const char* m_host;
    const char* m_rgb_camera_path;

    ToF* m_tof;
    cv::VideoCapture* m_videoCapture;
    bool m_rgb_available = false;
    cv::Mat rgb_image;
    std::mutex rgb_image_mutex;
};

class VsemiLimuNode : public rclcpp::Node
{
public:
    VsemiLimuNode() : Node("vsemi_limu_node")
    {
        auto qos = rclcpp::QoS(rclcpp::KeepLast(100)).reliable().durability_volatile();

        pointcloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("/vsemi_limu/pointcloud", qos);
        depth_image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("/vsemi_limu/depth_image", qos);
        rgb_image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("/vsemi_limu/rgb_image", qos);
        
        RCLCPP_INFO(this->get_logger(), "vsemi_limu node initialized.");
        
        // Initialize ToF camera
        m_limu = new LimuLidar(1, "10.10.31.180", "/dev/v4l/by-path/pci-0000:00:14.0-usb-0:6.1:1.0-video-index0");
        bool status = m_limu->start();
        m_limu->subscribeToF([this](std::shared_ptr<Frame> f) -> void {  updateFrame(f); });  // Fixed: capture 'this'
    }

    ~VsemiLimuNode()
    {
        m_limu->shutdown();
        delete m_limu;  // Added cleanup
    }

private:
    LimuLidar* m_limu;

    void updateFrame(std::shared_ptr<Frame> frame)
    {
        //RCLCPP_INFO(this->get_logger(), "updateFrame ...");

        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);

        // Convert frame data to point cloud
        if (frame->data_3d_xyz_rgb != nullptr && frame->n_points > 0) {
            pcl::PointXYZRGB* data_ptr = reinterpret_cast<pcl::PointXYZRGB*>(frame->data_3d_xyz_rgb);
            std::vector<pcl::PointXYZRGB> pts(data_ptr, data_ptr + frame->n_points);
            cloud->points.insert(cloud->points.end(), pts.begin(), pts.end());
            cloud->width = cloud->points.size();
            cloud->height = 1;
            cloud->is_dense = false;
        }
        
        // Process and store image
        if (frame->data_2d_bgr != nullptr) {
            cv::Mat depth_bgr(frame->height, frame->width, CV_8UC3, frame->data_2d_bgr);
            cv::flip(depth_bgr, depth_bgr, 1);
            
            //RCLCPP_INFO(this->get_logger(), "Frame updated with %zu points", cloud->points.size());

            publish_frame(cloud, depth_bgr);
        }
    }

    void publish_frame(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud, cv::Mat depth_bgr)
    {
        // Convert to ROS point cloud message
        sensor_msgs::msg::PointCloud2 cloud_msg;  // Use local variable instead of member
        pcl::toROSMsg(*cloud, cloud_msg);
        cloud_msg.header.frame_id = "vsemi_limu_frame";
        
        // Convert OpenCV image to ROS image message
        auto depth_image_msg = std::make_shared<sensor_msgs::msg::Image>();
        cv_bridge::CvImage cv_image_depth;
        cv_image_depth.encoding = "bgr8";
        cv_image_depth.image = depth_bgr;
        cv_image_depth.toImageMsg(*depth_image_msg);
        depth_image_msg->header.frame_id = "vsemi_limu_frame";

        auto now = this->get_clock()->now();
        
        // Update timestamps
        cloud_msg.header.stamp = now;
        depth_image_msg->header.stamp = now;

        // Publish messages
        pointcloud_pub_->publish(cloud_msg);
        depth_image_pub_->publish(*depth_image_msg);        

        if (m_limu->is_rgb_available())
        {
            auto rgb_image_msg = std::make_shared<sensor_msgs::msg::Image>();
            cv_bridge::CvImage cv_image_rgb;
            cv_image_rgb.encoding = "bgr8";
            cv_image_rgb.image = m_limu->current_rgb_image();
            cv_image_rgb.toImageMsg(*rgb_image_msg);
            rgb_image_msg->header.frame_id = "vsemi_limu_frame";
            rgb_image_msg->header.stamp = now;

            rgb_image_pub_->publish(*rgb_image_msg);
        }

        RCLCPP_DEBUG(this->get_logger(), "Published point cloud and image");
    }

    // Publishers
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pointcloud_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr depth_image_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr rgb_image_pub_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<VsemiLimuNode>();

    rclcpp::spin(node);
    
    g_ros_running = false;  // Signal the RGB thread to stop
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    std::cerr << "Shutdown ROS ... " << std::endl;

    rclcpp::shutdown();
    return 0;
}