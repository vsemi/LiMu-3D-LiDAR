#include "tof320.hpp"

#include <iostream>
#include <string>
#include "frame.hpp"

boost::asio::io_service ToF320::ioServiceUDP;
boost::scoped_ptr<boost::thread> ToF320::serverThreadUDP;

ToF320::ToF320(char* host, const char* port) : 
    tcpConnection(host, port, ioServiceTCP),
    isStreaming(false),
    dataType(0),
    currentFrame_id(0),
    data(Packet(25+320*240*4*2)),
    cameraIP(host)  // Store camera IP
{
    std::cout << "Creating ToF320 for IP: " << host << ":" << port << std::endl;

    if (!tcpConnection.connect()) {
        std::cerr << "WARNING: Failed to establish TCP connection to " << host << std::endl;
    } else {
        std::cout << "TCP connection verified for " << host << std::endl;
    }

    cartesianTransform.initLensTransform(sensorPixelSizeMM, width, height, lensCenterOffsetX, lensCenterOffsetY, lensType);
    old_lensCenterOffsetX = lensCenterOffsetX;
    old_lensCenterOffsetY = lensCenterOffsetY;
    old_lensType = lensType;

    imageColorizer = new ImageColorizer();
    imageColorizer->setRange(0, 20000);

    // Start io_service in its own thread
    serverThreadTCP.reset(new boost::thread([this]() {
        try {
            ioServiceTCP.run();
        } catch (const std::exception& e) {
            std::cerr << "io_service exception for " << cameraIP << ": " << e.what() << std::endl;
        }
    }));

    // Get the shared UDP server manager instance
    UdpServerManager& udpManager = UdpServerManager::getInstance(ioServiceUDP, serverThreadUDP);
    
    // Small delay to ensure UDP server is ready
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    
    // Subscribe to receive packets for this specific camera IP
    udpManager.subscribe(cameraIP, [&](const Packet& p) -> void
    {
        //std::cout << "   UDP data: " << cameraIP << std::endl;
        uint32_t packetNum  = (p[16] << 24) + (p[17] << 16) + (p[18] << 8) + p[19];
        uint32_t offset = (p[8] << 24) + (p[9] << 16) + (p[10] << 8) + p[11];
        uint16_t payloadSize = (p[6] << 8) + p[7];

        if(packetNum == 0) { // new frame
            uint16_t width  = (p[23] << 8) + p[24];
            uint16_t height = (p[25] << 8) + p[26];
            int payloadHeaderOffset = (p[43] << 8) + p[44];

            currentFrame = std::shared_ptr<Frame>(new Frame(dataType, currentFrame_id++, width, height, payloadHeaderOffset));
            memcpy(&data[offset], &p[Frame::UDP_HEADER_OFFSET], payloadSize);
            cameraInfoReady(getCameraInfo(p));

        } else {
            uint32_t numPackets = (p[12] << 24) + (p[13] << 16) + (p[14] << 8) + p[15];
            memcpy(&data[offset], &p[Frame::UDP_HEADER_OFFSET], payloadSize);

            if (packetNum == numPackets - 1) { //last frame                                
                currentFrame->sortData(data);  //copy data -> dist, ampl, dcs
                if (precompute) computeFrame(currentFrame);
                frameReady(currentFrame);
            }
        }
    });
    std::cout << "UDP subscribed: " << cameraIP << std::endl;
}

ToF320::~ToF320() {
    shutdown();
}

bool ToF320::isConnected() {
    return tcpConnection.isConnected();
}

int ToF320::getWidth()
{
    return width;
}
int ToF320::getHeight()
{
    return height;
}

void ToF320::setLensType(int t)
{
    lensType = t;
    if(old_lensType != lensType){
        cartesianTransform.initLensTransform(sensorPixelSizeMM, width, height, lensCenterOffsetX, lensCenterOffsetY, lensType);
        old_lensType = lensType;
    }
}
void ToF320::setLensCenter(int cx, int cy)
{
    lensCenterOffsetX = cx;
    lensCenterOffsetY = cy;
    if(old_lensCenterOffsetX != lensCenterOffsetX || old_lensCenterOffsetY != lensCenterOffsetY){
        cartesianTransform.initLensTransform(sensorPixelSizeMM, width, height, lensCenterOffsetX, lensCenterOffsetY, lensType);
        old_lensCenterOffsetX = lensCenterOffsetX;
        old_lensCenterOffsetY = lensCenterOffsetY;
    }
}

void ToF320::stopStream() {
    if (!isStreaming) { return; }
    std::vector<uint8_t> command({0x00, COMMAND_STOP_STREAM});
    tcpConnection.sendCommand(command);
    isStreaming = false;
}

void ToF320::shutdown() {
    stopStream();

    UdpServerManager& udpManager = UdpServerManager::getInstance(ioServiceUDP, serverThreadUDP);
    udpManager.unsubscribe(cameraIP);
    udpManager.shutdown();
    
    tcpConnection.close();

    ioServiceTCP.stop();
    ioServiceUDP.stop();

    serverThreadTCP->interrupt();
    serverThreadUDP->interrupt();
}

void ToF320::streamDCS()
{
    setDataType(Frame::DCS);
    streamMeasurement(COMMAND_GET_DCS);
}

void ToF320::streamDistanceAmplitude() {
    setDataType(Frame::AMPLITUDE);
    streamMeasurement(COMMAND_GET_DIST_AND_AMP);
}

void ToF320::streamDistance() {
    setDataType(Frame::DISTANCE);
    streamMeasurement(COMMAND_GET_DISTANCE);
}

void ToF320::streamGrayscale() {
    setDataType(Frame::GRAYSCALE);
    streamMeasurement(COMMAND_GET_GRAYSCALE);
}


void ToF320::setOffset(int16_t offset){
    std::vector<uint8_t> payload = {
        0x00, 0x14,
        static_cast<uint8_t>(offset >> 8),
        static_cast<uint8_t>(offset & 0x00ff)
    };

    tcpConnection.sendCommand(payload);
}

void ToF320::setMinAmplitude(uint16_t minAmplitude){
    std::vector<uint8_t> payload = {
        0x00, 0x15,
        static_cast<uint8_t>(minAmplitude >> 8),
        static_cast<uint8_t>(minAmplitude & 0x00ff)
    };

    tcpConnection.sendCommand(payload);
}

void ToF320::setBinning(const bool vertical, const bool horizontal)
{
    uint8_t  byte = 0;
    if(vertical && horizontal){
        byte = 3;
    }else if(vertical){
        byte = 1;
    }else if(horizontal){
        byte = 2;
    }

    std::vector<uint8_t> payload = {
        0x00, 0x18, byte
    };

    tcpConnection.sendCommand(payload);
}


void ToF320::setRoi(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1)
{
    std::vector<uint8_t> payload = {
        0x00, 0x00,
        static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0 & 0x00ff),
        static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0 & 0x00ff),
        static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1 & 0x00ff),
        static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1 & 0x00ff)};
    tcpConnection.sendCommand(payload);
}


void ToF320::setIntegrationTime(uint16_t low, uint16_t mid, uint16_t high, uint16_t gray)
{
    std::vector<uint8_t> payload = {
        0x00, 0x01,
        static_cast<uint8_t>(low >> 8), static_cast<uint8_t>(low & 0x00ff),
        static_cast<uint8_t>(mid >> 8), static_cast<uint8_t>(mid & 0x00ff),
        static_cast<uint8_t>(high >> 8), static_cast<uint8_t>(high & 0x00ff),
        static_cast<uint8_t>(gray >> 8), static_cast<uint8_t>(gray & 0x00ff)};
    tcpConnection.sendCommand(payload);
}

void ToF320::setHDRMode(uint8_t mode)
{
    std::vector<uint8_t> payload;
    uint16_t command = COMMAND_SET_HDR;

    insertValue(payload, command);

    payload.push_back(mode);

    tcpConnection.sendCommand(payload);
}

void ToF320::setIPAddress(IPAddress ip, IPAddress netmask, IPAddress gateway)
{
    std::vector<uint8_t> payload;
    uint16_t command = COMMAND_SET_IP;

    insertValue(payload, command);

    payload.push_back(ip.ip0);
    payload.push_back(ip.ip1);
    payload.push_back(ip.ip2);
    payload.push_back(ip.ip3);

    payload.push_back(netmask.ip0);
    payload.push_back(netmask.ip1);
    payload.push_back(netmask.ip2);
    payload.push_back(netmask.ip3);

    payload.push_back(gateway.ip0);
    payload.push_back(gateway.ip1);
    payload.push_back(gateway.ip2);
    payload.push_back(gateway.ip3);

    tcpConnection.sendCommand(payload);

    //std::string ip_address = std::to_string(ip.ip0) + "." + std::to_string(ip.ip1) + "." + std::to_string(ip.ip2) + "." + std::to_string(ip.ip3);
    //tcpConnection.setHost(const_cast<char*>(ip_address.c_str()));
}

void ToF320::setFilter(const bool medianFilter, const bool averageFilter, const uint16_t temporalFactor, const uint16_t temporalThreshold, const uint16_t edgeThreshold, const uint16_t temporalEdgeThresholdLow, const uint16_t temporalEdgeThresholdHigh, const uint16_t interferenceDetectionLimit, const bool interferenceDetectionUseLastValue)
{
    std::vector<uint8_t> payload;
    uint16_t command = COMMAND_SET_FILTER;

    //Insert the 16Bit command
    insertValue(payload, command);

    //Insert temporal filter factor
    insertValue(payload, temporalFactor);

    //Insert temporal filter threshold
    insertValue(payload, temporalThreshold);

    //Insert median filter
    insertValue8(payload, boolToUint8(medianFilter));

    //Insert average filter
    insertValue8(payload, boolToUint8(averageFilter));

    //Insert edge filter threshold
    insertValue(payload, edgeThreshold);

    //Insert interference detection use last value flag
    insertValue8(payload, boolToUint8(interferenceDetectionUseLastValue));

    //Insert edge filter interference detection limit
    insertValue(payload, interferenceDetectionLimit);

    //Insert edge filter threshold low
    insertValue(payload, temporalEdgeThresholdLow);

    //Insert edge filter threshold high
    insertValue(payload, temporalEdgeThresholdHigh);

    tcpConnection.sendCommand(payload);
}


void ToF320::setModulation(const uint8_t index, const uint8_t channel){

    std::vector<uint8_t> payload;
    uint16_t command = COMMAND_SET_MODULATION;

    insertValue(payload, command);

    uint8_t data = static_cast<char>(index);
    payload.push_back(data);

    data = static_cast<char>(channel);
    payload.push_back(data);

    data = 0; //AutoChannel reserved
    payload.push_back(data);

    tcpConnection.sendCommand(payload);
}


void ToF320::insertValue8(std::vector<uint8_t> &output, const uint8_t value){
    output.push_back(value);
}

void ToF320::insertValue(std::vector<uint8_t> &output, const int16_t value)
{
    output.push_back(value >> 8);
    output.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint8_t ToF320::boolToUint8(const bool value)
{
    if (value)  return 1;
    else        return 0;
}

void ToF320::subscribeFrame(std::function<void (std::shared_ptr<Frame>)> onFrameReady)
{
    frameReady.connect(onFrameReady);
}

void ToF320::subscribeCameraInfo(std::function<void (std::shared_ptr<CameraInfo>)> onCameraInfoReady)
{
    cameraInfoReady.connect(onCameraInfoReady);
}

std::shared_ptr<CameraInfo> ToF320::getCameraInfo(const Packet& p) {
    std::shared_ptr<CameraInfo> camInfo(new CameraInfo);

    int offset = 23;
    camInfo->width  = (p[offset++] << 8) + p[offset++];
    camInfo->height = (p[offset++] << 8) + p[offset++];
    camInfo->roiX0  = (p[offset++] << 8) + p[offset++];
    camInfo->roiY0  = (p[offset++] << 8) + p[offset++];
    camInfo->roiX1  = (p[offset++] << 8) + p[offset++];
    camInfo->roiY1  = (p[offset++] << 8) + p[offset++];

    return camInfo;
}

void ToF320::setDataType(uint8_t d) {
    dataType = d;
}

void ToF320::streamMeasurement(uint8_t cmd) {
    tcpConnection.sendCommand(std::vector<uint8_t>({0x00, cmd, 0x01}));
    isStreaming = true;
}
void ToF320::computeFrame(std::shared_ptr<Frame> frame)
{
    int x, y, k, l;

    uint16_t distance, amplitude;
    double px, py, pz;
    double max_X = 0.0, z_max_X = 0.0;
    double min_X = 1000.0, z_min_X = 0.0;

    Color c;
    int rgb;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    int i_data_point = 0, i_2d_color = 0, i_point_3d = 0, i_out_index = 0, p_index = 0;

    float amplitude_min = 100000.0, amplitude_max = -100000.0;
    float X, Y, Z;
    bool valid_point = false;

    for(k=0, l=0, y=0; y< frame->height; y++){
        for(x=0; x< frame->width; x++, k++, l+=2){
            valid_point = false;
            distance = (frame->distData[l+1] << 8) + frame->distData[l];

            c = imageColorizer->getColor(distance);

            r = c.r;
            g = c.g;
            b = c.b;

            rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);

            if(frame->dataType == Frame::AMPLITUDE)
            {
                amplitude = (frame->amplData[l+1] << 8)  + frame->amplData[l];
                if (amplitude_min > amplitude) amplitude_min = amplitude;
                if (amplitude_max < amplitude) amplitude_max = amplitude;
                
                frame->data_amplitude[i_out_index] = static_cast<float>(amplitude);
            }

            if(frame->dataType == Frame::GRAYSCALE)
            {
                amplitude = (frame->amplData[l+1] << 8)  + frame->amplData[l];
                
                frame->data_grayscale[i_out_index] = static_cast<uint8_t>(amplitude);
            }

            if (distance > 0 && distance < LOW_AMPLITUDE){
                cartesianTransform.transformPixel(x, y, distance, px, py, pz);
                X = -px / 1000.0;
                Y = py / 1000.0;
                Z = pz / 1000.0;
                
                frame->saturated_mask[i_out_index]     = 0;

                frame->data_depth[i_out_index]         = Z;

                frame->data_2d_bgr[i_2d_color]         = b;
                frame->data_2d_bgr[i_2d_color + 1]     = g;
                frame->data_2d_bgr[i_2d_color + 2]     = r;

                frame->data_3d_xyz_rgb[i_point_3d]     = X;
                frame->data_3d_xyz_rgb[i_point_3d + 1] = Y;
                frame->data_3d_xyz_rgb[i_point_3d + 2] = Z;
                frame->data_3d_xyz_rgb[i_point_3d + 3] = 0.0f;
                frame->data_3d_xyz_rgb[i_point_3d + 4] = *reinterpret_cast<float*>(&rgb);
                frame->data_3d_xyz_rgb[i_point_3d + 5] = *reinterpret_cast<float*>(&r); //r;
                frame->data_3d_xyz_rgb[i_point_3d + 6] = *reinterpret_cast<float*>(&g); //g;
                frame->data_3d_xyz_rgb[i_point_3d + 7] = *reinterpret_cast<float*>(&b); //b;

                valid_point = true;

            } else if (distance == LOW_AMPLITUDE)
            {
                frame->saturated_mask[i_out_index]     = 0;

                frame->data_2d_bgr[i_2d_color]         = b;
                frame->data_2d_bgr[i_2d_color + 1]     = g;
                frame->data_2d_bgr[i_2d_color + 2]     = r;

            } else if (distance == ADC_OVERFLOW || distance == SATURATION)
            {
                frame->saturated_mask[i_out_index]     = 255;

                frame->data_2d_bgr[i_2d_color]         = b;
                frame->data_2d_bgr[i_2d_color + 1]     = g;
                frame->data_2d_bgr[i_2d_color + 2]     = r;
            } else {
                frame->saturated_mask[i_out_index]     = 0;

                frame->data_2d_bgr[i_2d_color]         = b;
                frame->data_2d_bgr[i_2d_color + 1]     = g;
                frame->data_2d_bgr[i_2d_color + 2]     = r;
            }
            if (! valid_point)
            {

                frame->data_depth[i_out_index]         = std::numeric_limits<float>::quiet_NaN();

                frame->data_2d_bgr[i_2d_color]         = 0;
                frame->data_2d_bgr[i_2d_color + 1]     = 0;
                frame->data_2d_bgr[i_2d_color + 2]     = 0;

                frame->data_3d_xyz_rgb[i_point_3d]     = std::numeric_limits<float>::quiet_NaN();
                frame->data_3d_xyz_rgb[i_point_3d + 1] = std::numeric_limits<float>::quiet_NaN();
                frame->data_3d_xyz_rgb[i_point_3d + 2] = std::numeric_limits<float>::quiet_NaN();
                frame->data_3d_xyz_rgb[i_point_3d + 3] = std::numeric_limits<float>::quiet_NaN();
                frame->data_3d_xyz_rgb[i_point_3d + 4] = std::numeric_limits<float>::quiet_NaN();
                frame->data_3d_xyz_rgb[i_point_3d + 5] = std::numeric_limits<float>::quiet_NaN();
                frame->data_3d_xyz_rgb[i_point_3d + 6] = std::numeric_limits<float>::quiet_NaN();
                frame->data_3d_xyz_rgb[i_point_3d + 7] = std::numeric_limits<float>::quiet_NaN();

            }
            
            i_data_point ++;
            i_point_3d += 8;
            i_2d_color += 3;
            i_out_index ++;
        }
    }
}
    
void ToF320::setPrecompute(bool prec)
{
    this->precompute = prec;
}

bool ToF320::isPrecomputed()
{
    return this->precompute;
}