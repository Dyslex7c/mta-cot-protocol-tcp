#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include <string>
#include "mta_protocol.h"
#include "protobuf_handler.h"

using boost::asio::ip::tcp;

class MTAServer {
public:
    MTAServer(boost::asio::io_context& io_context, short port, uint32_t y_share = 0);

private:
    void start_accept();

    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(boost::asio::io_context& io_context, 
                MTAProtocol& mta_protocol, 
                MTAProtobufHandler& protobuf_handler,
                uint32_t y_share);

        tcp::socket& socket();
        void start();

    private:
        enum class ProtocolState {
            WAITING_FOR_CORRELATION_DELTA,
            SENDING_BOB_SETUP,
            WAITING_FOR_ALICE_MESSAGES,
            SENDING_BOB_MESSAGES,
            PROTOCOL_COMPLETE
        };

        // Network I/O methods
        void read_message_with_size();
        void send_message_with_size(const std::vector<uint8_t>& message);
        
        // Protocol message processing
        void process_received_message();
        void process_correlation_delta(const std::vector<uint8_t>& data);
        void process_alice_messages(const std::vector<uint8_t>& data);
        
        void send_bob_setup();
        void send_bob_messages();

        tcp::socket socket_;
        MTAProtocol& mta_protocol_;
        MTAProtobufHandler& protobuf_handler_;
        
        ProtocolState state_;
        uint32_t bob_y_share_;              // Bob's multiplicative share
        uint32_t bob_additive_share_;       // Bob's computed additive share
        uint32_t correlation_delta_;        // Correlation delta received from Alice
        uint32_t bob_correlation_check_;    // Correlation check value for verification
        
        // protocol data structures - using consistent types from MTAProtocol
        MTAProtocol::BobSetup bob_setup_;
        MTAProtocol::BobMessages bob_messages_; //Holds prepared Bob messages with correct beta
        
        std::vector<uint8_t> read_buffer_;
        std::vector<uint8_t> write_buffer_;
        uint32_t last_message_size_;
    };

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::unique_ptr<MTAProtocol> mta_protocol_;
    std::unique_ptr<MTAProtobufHandler> protobuf_handler_;
    uint32_t bob_y_share_;
};