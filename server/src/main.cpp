#include <iostream>
#include <boost/asio.hpp>
#include <random>
#include "tcp/mta_server.h"

int main(int argc, char* argv[]) {
    try {
        int port = 8080;
        uint32_t bob_share = 0;
        
        if (argc >= 2) {
            port = std::atoi(argv[1]);
            if (port <= 0 || port > 65535) {
                std::cerr << "Invalid port number: " << port << std::endl;
                return 1;
            }
        }
        
        if (argc >= 3) {
            bob_share = static_cast<uint32_t>(std::atoi(argv[2]));
        }
        
        std::cout << "Usage: " << argv[0] << " [port] [bob_multiplicative_share]" << std::endl;
        std::cout << "Port: " << port << std::endl;
        
        if (bob_share == 0) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> dis(1, 1000000);
            bob_share = dis(gen);
            std::cout << "Generated random Bob's multiplicative share: " << bob_share << std::endl;
        } else {
            std::cout << "Using provided Bob's multiplicative share: " << bob_share << std::endl;
        }
                
        boost::asio::io_context io_context;
        
        MTAServer server(io_context, static_cast<short>(port), bob_share);
        
        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
        std::cout << "Waiting for Alice (client) to connect...\n" << std::endl;
        
        io_context.run();
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}