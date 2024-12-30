#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <string>

boost::asio::io_context io_context;
boost::asio::ip::tcp::resolver resolver(io_context);
boost::asio::ip::tcp::socket sock(io_context);
boost::array<char, 4096> buffer;

void read_handler(const boost::system::error_code &ec, std::size_t bytes_transferred) {
    if (!ec) {
        std::cout << std::string(buffer.data(), bytes_transferred) << std::endl;
        sock.async_read_some(boost::asio::buffer(buffer), read_handler);
    }
}

void connect_handler(const boost::system::error_code &ec, const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
    if (!ec) {
        // 构造 HTTP GET 请求
        std::string request = "GET / HTTP/1.1\r\n"
                              "Host: highscore.de\r\n"
                              "Connection: close\r\n"
                              "\r\n";
        boost::asio::write(sock, boost::asio::buffer(request));
        sock.async_read_some(boost::asio::buffer(buffer), read_handler);
    }
}

void resolve_handler(const boost::system::error_code &ec,  const boost::asio::ip::tcp::resolver::results_type results) {
    if(!ec) {
        boost::asio::async_connect(sock, results, connect_handler);
    }
}

int main() {
    resolver.async_resolve("www.highscore.de", "80", resolve_handler);
    io_context.run();
    return 0;
}