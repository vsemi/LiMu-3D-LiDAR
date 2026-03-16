#ifndef __UDPSERVERMANAGER_H__
#define __UDPSERVERMANAGER_H__

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>

using boost::asio::ip::udp;

typedef std::vector<uint8_t> Packet;

class UdpServerManager {
public:
    static const int UDP_PORT = 45454;
    static const int RECV_BUFF_SIZE = 2048;
    
    UdpServerManager(boost::asio::io_service &ios);
    ~UdpServerManager();
    
    // Get singleton instance
    static UdpServerManager& getInstance(boost::asio::io_service &ios, boost::scoped_ptr<boost::thread> &serverThreadUDP);
    
    // Subscribe a camera to receive packets from a specific IP
    boost::signals2::connection subscribe(const std::string& cameraIP, std::function<void(const Packet&)> callback);
    
    // Unsubscribe a camera
    void unsubscribe(const std::string& cameraIP);
    
    // Check if server is running
    bool isRunning() const { return running; }
    
    // Shutdown the manager
    void shutdown();

private:
    
    static std::unique_ptr<UdpServerManager> instance;
    static std::mutex instanceMutex;
    
    boost::asio::io_service& ioService;
    udp::socket socket;
    udp::endpoint remoteEndpoint;
    Packet recvBuffer;
    bool running;
    
    // Map camera IP to their callback functions
    std::unordered_map<std::string, std::shared_ptr<boost::signals2::signal<void (const Packet&)>>> cameraCallbacks;
    std::mutex callbackMutex;
    
    void startReceive();
    void handleReceive(const boost::system::error_code &error, std::size_t bytesReceived);
};

#endif