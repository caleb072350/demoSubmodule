#include <boost/asio.hpp>
#include <string>
#include <iostream>

boost::asio::io_context io_context;
boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 80);
boost::asio::ip::tcp::acceptor acceptor(io_context, endpoint);
boost::asio::ip::tcp::socket sock(io_context);
std::string data = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";

void write_handler(const boost::system::error_code &ec, std::size_t bytes_transferred) 
{ 
    std::cout << "write " << bytes_transferred << " data!" << std::endl;
}


void accept_handler(const boost::system::error_code &ec) 
{ 
  if (!ec) 
  { 
    boost::asio::async_write(sock, boost::asio::buffer(data), write_handler); 
    sock = boost::asio::ip::tcp::socket(io_context);
    acceptor.async_accept(sock, accept_handler);
  } 
} 

int main() {
    acceptor.listen();
    acceptor.async_accept(sock, accept_handler);
    io_context.run();
}