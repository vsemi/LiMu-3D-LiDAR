#ifndef __TOF320_H__
#define __TOF320_H__

#include <boost/thread.hpp>
#include "frame.hpp"
#include "camera_info.hpp"
#include "tcp_connection.hpp"
#include "udp_server_manager.hpp"
#include "tof.hpp"

#include "cartesian_transform.hpp"
#include "color.hpp"

typedef std::vector<uint8_t> Packet;

class ToF320 : public ToF 
{
public:
  ToF320(char* host, const char* port);
  ~ToF320();

  void stopStream();  
  void shutdown();  
  void streamDCS();
  void streamGrayscale();
  void streamDistance();
  void streamDistanceAmplitude();
  void setOffset(int16_t offset);
  void setMinAmplitude(uint16_t minAmplitude);
  void setBinning(const bool vertical, const bool horizontal);
  void setRoi(const uint16_t x0, const uint16_t y0, const uint16_t x1, const uint16_t y1);
  void setIntegrationTime(uint16_t, uint16_t, uint16_t, uint16_t);
  void setHDRMode(uint8_t mode);
  void setModulation(const uint8_t index, const uint8_t channel);
  void setFilter(const bool medianFilter, const bool averageFilter, const uint16_t temporalFactor, const uint16_t temporalThreshold, const uint16_t edgeThreshold,
                 const uint16_t temporalEdgeThresholdLow, const uint16_t temporalEdgeThresholdHigh, const uint16_t interferenceDetectionLimit, const bool interferenceDetectionUseLastValue);

  void setIPAddress(IPAddress ip, IPAddress netmask, IPAddress gateway);

  void subscribeFrame(std::function<void (std::shared_ptr<Frame>)>);
  void subscribeCameraInfo(std::function<void (std::shared_ptr<CameraInfo>)>);
  std::shared_ptr<CameraInfo> getCameraInfo(const Packet &);

  void setLensType(int lensType);
  void setLensCenter(int cx, int cy);

  int getWidth();
  int getHeight();

  void computeFrame(std::shared_ptr<Frame> frame);

  void setPrecompute(bool precompute);
  bool isPrecomputed();

  bool isConnected();

private:
  bool isStreaming;
  uint8_t dataType;
  uint64_t currentFrame_id;  

  const static uint8_t COMMAND_SET_INT_TIMES = 1;
  const static uint8_t COMMAND_GET_DIST_AND_AMP = 2;
  const static uint8_t COMMAND_GET_DISTANCE = 3;
  const static uint8_t COMMAND_GET_GRAYSCALE = 5;
  const static uint8_t COMMAND_STOP_STREAM = 6;
  const static uint8_t COMMAND_GET_DCS = 7;
  const static uint8_t COMMAND_CALIBRATE = 30;
  const static uint8_t COMMAND_SET_FILTER = 22;
  const static uint8_t COMMAND_SET_MODULATION = 23;
  const static uint8_t COMMAND_SET_BINNING = 24;
  const static uint16_t COMMAND_SET_HDR = 25;
  const static uint16_t COMMAND_SET_IP = 40;

  Packet data;
  std::shared_ptr<Frame> currentFrame;

  boost::asio::io_service ioServiceTCP;
  boost::scoped_ptr<boost::thread> serverThreadTCP;

  static boost::asio::io_service ioServiceUDP;
  static boost::scoped_ptr<boost::thread> serverThreadUDP;

  boost::signals2::signal<void (std::shared_ptr<Frame>)> frameReady;
  boost::signals2::signal<void (std::shared_ptr<CameraInfo>)> cameraInfoReady;
  TcpConnection tcpConnection;
  std::string cameraIP;

  void setDataType(uint8_t);  
  void streamMeasurement(uint8_t);
  void insertValue8(std::vector<uint8_t> &output, const uint8_t value);
  void insertValue(std::vector<uint8_t> &output, const int16_t value);
  uint8_t boolToUint8(const bool value);

  CartesianTransform cartesianTransform;

  const int width   = 320;
  const int height  = 240;
  const double sensorPixelSizeMM = 0.02; //camera sensor pixel size 20x20 um

  int lensType = 1;  //0- wide field, 1- standard field, 2 - narrow field
  int old_lensType = 1;

  int lensCenterOffsetX = 0;
  int lensCenterOffsetY = 0;
  int old_lensCenterOffsetX = 0;
  int old_lensCenterOffsetY = 0;

  ImageColorizer *imageColorizer;

  bool precompute = true;
};

#endif
