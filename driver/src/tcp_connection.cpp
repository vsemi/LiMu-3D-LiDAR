#include "tcp_connection.hpp"

#include <iostream>
#include <thread>

using boost::asio::ip::tcp;

typedef std::vector<uint8_t> Packet;

const int COMMAND_MAX_ATTEMPTS = 30;
const int CONNECT_TIMEOUT_MINI_SECONDS = 30000;

TcpConnection::TcpConnection(const char* h, const char* p, boost::asio::io_service& ioService)
  : host(h), 
    port(p), 
    resolver(ioService), 
    socket(nullptr),
    ioService(ioService),
    state(STATE_DISCONNECTED),
    previousState(STATE_DISCONNECTED) {
}

TcpConnection::~TcpConnection() {
  try {
    close();
  } catch (const boost::system::system_error& e) {
    std::cerr << e.what() << std::endl;
  }
}

void TcpConnection::sendCommand(const std::vector<uint8_t>& data) {
  if (!isConnected() || !socket) return;

  uint32_t data_len = data.size();
  
  std::vector<uint8_t> buffer;
  buffer.reserve(MARKER_SIZE + sizeof(data_len) + data_len + MARKER_SIZE);
  
  // Add START_MARKER
  for (int i = 0; i < MARKER_SIZE; ++i) {
    buffer.push_back(static_cast<uint8_t>(START_MARKER[i]));
  }
  
  // Add data length
  buffer.push_back(static_cast<uint8_t>((data_len >> 24) & 0xff));
  buffer.push_back(static_cast<uint8_t>((data_len >> 16) & 0xff));
  buffer.push_back(static_cast<uint8_t>((data_len >>  8) & 0xff));
  buffer.push_back(static_cast<uint8_t>((data_len >>  0) & 0xff));
  
  // Add data
  buffer.insert(buffer.end(), data.begin(), data.end());
  
  // Add END_MARKER
  for (int i = 0; i < MARKER_SIZE; ++i) {
    buffer.push_back(static_cast<uint8_t>(END_MARKER[i]));
  }

  std::cout << "TCP send command to " << host << std::endl;

  int attempts = 0;
  while (attempts < COMMAND_MAX_ATTEMPTS) {
    try {
      boost::system::error_code error;
      
      size_t bytes_written = boost::asio::write(*socket, boost::asio::buffer(buffer), 
                                                 boost::asio::transfer_all(), error);
      
      if (error) {
        throw boost::system::system_error(error);
      }
      
      std::cout << "   TCP sent " << bytes_written << " bytes to " << host << std::endl;
      waitAck();
      break;
    } catch (const boost::system::system_error& e) {
      std::cout << "   TCP send command to " << host << " failed: " << e.what() << ", retry..." << std::endl;
      
      // If connection is dead, try to reconnect
      if (e.code() == boost::asio::error::eof || 
          e.code() == boost::asio::error::connection_reset) {
        std::cout << "   Connection lost, attempting to reconnect..." << std::endl;
        connect();
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    attempts++;
  }
}

bool TcpConnection::connect() {
  if (isConnected()) return true;

  std::cout << "TCP connecting to " << host << ":" << port << std::endl;

  updateState(STATE_CONNECTING);
  
  // Clean up any existing socket
  cleanupSocket();
  
  // Create new socket
  socket = std::make_unique<tcp::socket>(ioService);
  
  tcp::resolver::query query(host, port);
  std::cout << "   TCP resolving " << host << ":" << port << std::endl;
  
  boost::system::error_code error;
  tcp::resolver::iterator endpoint_iterator;
  
  try {
    endpoint_iterator = resolver.resolve(query);
  } catch (const boost::system::system_error& e) {
    std::cout << "   TCP resolve failed: " << e.what() << std::endl;
    updateState(STATE_DISCONNECTED);
    return false;
  }
  
  tcp::resolver::iterator end;
  
  // Set socket options before connecting
  boost::system::error_code option_ec;
  socket->set_option(tcp::no_delay(true), option_ec); // Disable Nagle's algorithm
  socket->set_option(boost::asio::socket_base::reuse_address(true), option_ec);
  
  // Set a timeout for connection (optional)
  socket->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{ CONNECT_TIMEOUT_MINI_SECONDS }, option_ec);
  socket->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_SNDTIMEO>{ CONNECT_TIMEOUT_MINI_SECONDS }, option_ec);
  
  error = boost::asio::error::host_not_found;
  
  while (error && endpoint_iterator != end) {
    socket->close(); // Close if was open from previous attempt
    socket->connect(*endpoint_iterator++, error);
  }
  
  if (error) {
    std::cout << "   TCP connection error to " << host << ":" << port << ": " 
              << error.message() << std::endl;
    updateState(STATE_DISCONNECTED);
    return false;
  }
  
  updateState(STATE_CONNECTED);
  std::cout << "   TCP connected to " << host << ":" << port << std::endl;
  
  return true;
}

void TcpConnection::waitAck() {
  if (!socket || !socket->is_open()) {
    throw std::runtime_error("Socket not connected");
  }
  
  std::cout << "TCP waiting for ACK from " << host << std::endl;

  std::vector<uint8_t> buf(ACK_BUF_SIZE);
  boost::system::error_code error;

  updateState(STATE_WAIT_ACK);
  
  size_t len = 0;
  
  try {
    std::cout << "   Before read_some for " << host << ", socket state: " << (socket->is_open() ? "open" : "closed") << std::endl;
          
    len = socket->read_some(boost::asio::buffer(buf), error);
    
    if (error == boost::asio::error::eof) {
      std::cout << "   TCP connection closed by peer for " << host << std::endl;
      updateState(STATE_DISCONNECTED);
      throw std::runtime_error("Connection closed by peer");
    } else if (error) {
      throw boost::system::system_error(error);
    }
    
    std::cout << "   TCP ACK received from " << host << " (" << len << " bytes)" << std::endl;
    
    // Print received data for debugging
    std::cout << "A   CK data: ";
    for (size_t i = 0; i < len && i < 16; i++) {
      printf("%02x ", buf[i]);
    }
    std::cout << std::endl;
    
  } catch (const boost::system::system_error& e) {
    std::cout << "   TCP ACK read error from " << host << ": " << e.what() << std::endl;
    revertState();
    throw;
  }
  
  revertState();
}

void TcpConnection::cleanupSocket() {
  if (!socket) return;
  
  boost::system::error_code ec;
  
  if (socket->is_open()) {
    // Cancel any pending operations
    socket->cancel(ec);
    
    // Shutdown both send and receive
    socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    
    // Close the socket
    socket->close(ec);
  }
}

void TcpConnection::close() {
  disconnect();
  cleanupSocket();
  socket.reset();  // Destroy the socket
}

void TcpConnection::disconnect() {
  if (isDisconnected()) return;

  updateState(STATE_CLOSING);

  if (socket && socket->is_open()) {
    boost::system::error_code error;
    
    socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
    if (error && error != boost::asio::error::not_connected) {
      std::cerr << "Shutdown error for " << host << ": " << error.message() << std::endl;
    }
    
    socket->close(error);
    if (error) {
      std::cerr << "Close error for " << host << ": " << error.message() << std::endl;
    }
  }
  
  updateState(STATE_DISCONNECTED);
  std::cout << "TCP disconnected from " << host << std::endl;
}

void TcpConnection::updateState(State newState) const {
  previousState = state;
  state = newState;
}

void TcpConnection::revertState() const {
  state = previousState;
}

bool TcpConnection::isConnected() const {
  if (!socket) return false;
  
  // Check if socket is actually connected
  boost::system::error_code ec;
  socket->non_blocking(true); // Temporarily set non-blocking to check
  boost::asio::ip::tcp::endpoint endpoint = socket->remote_endpoint(ec);
  socket->non_blocking(false); // Restore blocking mode
  
  bool socket_connected = !ec && socket->is_open() && state == STATE_CONNECTED;
  
  return socket_connected;
}

bool TcpConnection::isDisconnected() const {
  return state == STATE_DISCONNECTED;
}