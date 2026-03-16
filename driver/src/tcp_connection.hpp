#ifndef __TCPCONNECTION_H__
#define __TCPCONNECTION_H__

#include <boost/asio.hpp>
#include <memory>  // Add this for std::unique_ptr

using boost::asio::ip::tcp;

class TcpConnection {
  static const int MARKER_SIZE = 4;
  static const int ACK_BUF_SIZE = 128;
  static constexpr const char* END_MARKER = "\xff\xff\x55\xaa";
  static constexpr const char* START_MARKER = "\xff\xff\xaa\x55";

public:
  enum State {
    STATE_CONNECTING,
    STATE_DISCONNECTED,
    STATE_CONNECTED,
    STATE_CLOSING,
    STATE_WAIT_ACK
  };

  TcpConnection(const char* host, const char* port, boost::asio::io_service &);
  ~TcpConnection();

  void sendCommand(const std::vector<uint8_t> &);

  const char* getHost() const { return host; }

  bool connect();
  bool isConnected() const;
  void close();

private:
  mutable State state, previousState;
  std::unique_ptr<tcp::socket> socket;
  tcp::resolver resolver;
  const char* host;
  const char* port;
  boost::asio::io_service& ioService;

  void waitAck();
  void updateState(State) const;
  void revertState() const;
  void disconnect();
  bool isDisconnected() const;
  void cleanupSocket();
};

#endif // __TCPCONNECTION_H__