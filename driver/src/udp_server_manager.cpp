#include "udp_server_manager.hpp"
#include <iostream>

using boost::asio::ip::udp;

std::unique_ptr<UdpServerManager> UdpServerManager::instance = nullptr;
std::mutex UdpServerManager::instanceMutex;

UdpServerManager& UdpServerManager::getInstance(boost::asio::io_service &ios, boost::scoped_ptr<boost::thread> &serverThreadUDP) {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance = std::unique_ptr<UdpServerManager>(new UdpServerManager(ios));
        std::cout << "UDP Server new instance created " << std::endl;

        serverThreadUDP.reset(new boost::thread([&ios]() {
            try {
                ios.run();
            } catch (const std::exception& e) {
                std::cerr << "io_service exception for: " << e.what() << std::endl;
            }
        }));
        std::cout << "UDP Server instance thread started " << std::endl;
    } else {
        std::cout << "UDP Server instance already exists, reusing" << std::endl;
    }
    return *instance;
}

UdpServerManager::UdpServerManager(boost::asio::io_service &ios) 
    : ioService(ios), 
      socket(ios, udp::endpoint(udp::v4(), UDP_PORT)),
      recvBuffer(Packet(RECV_BUFF_SIZE)),
      running(true) {
    std::cout << "UDP Server Manager started on port " << UDP_PORT << std::endl;
    startReceive();
}

UdpServerManager::~UdpServerManager() {
    shutdown();
}

void UdpServerManager::shutdown() {
    std::cerr << "Shutdown UdpServerManager ... " << std::endl;

    if (!running) return;
    
    running = false;
    
    std::lock_guard<std::mutex> lock(callbackMutex);
    cameraCallbacks.clear();
    
    boost::system::error_code error;
    socket.close(error);
    if (error) {
        std::cerr << "Error closing UDP socket: " << error.message() << std::endl;
    }
    std::cerr << "UdpServerManager socket closed." << std::endl;
}

void UdpServerManager::startReceive() {
    if (!running) return;

    socket.async_receive_from(
        boost::asio::buffer(recvBuffer), 
        remoteEndpoint,
        boost::bind(&UdpServerManager::handleReceive, 
                   this,
                   boost::asio::placeholders::error,
                   boost::asio::placeholders::bytes_transferred)
    );
}

void UdpServerManager::handleReceive(const boost::system::error_code& error, 
                                     std::size_t bytesReceived) {
    if (!error) {
        std::string remoteIP = remoteEndpoint.address().to_string();
        //std::cout << "UDP received from: " << remoteIP << " (" << bytesReceived << " bytes)" << std::endl;
        
        Packet packetCopy(recvBuffer.begin(), recvBuffer.begin() + bytesReceived);        
        {
            std::lock_guard<std::mutex> lock(callbackMutex);            
            // Debug: list all registered callbacks
            //  std::cout << "Registered camera IPs (" << cameraCallbacks.size() << "): ";
            //  for (const auto& pair : cameraCallbacks) {
            //      std::cout << pair.first << " ";
            //  }
            //  std::cout << std::endl;
            auto it = cameraCallbacks.find(remoteIP);
            if (it != cameraCallbacks.end()) {
                //std::cout << "Notifying callback for IP: " << remoteIP << std::endl;
                (*(it->second))(packetCopy);
            } else {
                //std::cout << "WARNING: No callback registered for IP: " << remoteIP << std::endl;
            }
        }
        
        startReceive();
    } else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "UDP receive error: " << error.message() << std::endl;
        if (running) {
            startReceive();
        }
    }
}

boost::signals2::connection UdpServerManager::subscribe(const std::string& cameraIP, 
                                                        std::function<void(const Packet&)> callback) {
    std::cout << "Camera subscribing for UDP: " << cameraIP << std::endl;
    
    std::lock_guard<std::mutex> lock(callbackMutex);
    
    // Create signal if it doesn't exist
    if (cameraCallbacks.find(cameraIP) == cameraCallbacks.end()) {
        cameraCallbacks[cameraIP] = std::make_shared<boost::signals2::signal<void (const Packet&)>>();
        std::cout << "   Created new signal for: " << cameraIP << std::endl;
    } else {
        std::cout << "   Using existing signal for: " << cameraIP << std::endl;
    }
    
    // Connect the callback (multiple callbacks can be connected to the same signal)
    return cameraCallbacks[cameraIP]->connect(callback);
}

void UdpServerManager::unsubscribe(const std::string& cameraIP) {
    std::cout << "Camera unsubscribed from UDP: " << cameraIP << std::endl;
    
    std::lock_guard<std::mutex> lock(callbackMutex);
    cameraCallbacks.erase(cameraIP);
}