#include "mta_server.h"
#include "protobuf_handler.h"
#include <iostream>
#include <iomanip>
#include <random>

MTAServer::MTAServer(boost::asio::io_context& io_context, short port, uint32_t y_share)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      mta_protocol_(std::make_unique<MTAProtocol>()),
      protobuf_handler_(std::make_unique<MTAProtobufHandler>()),
      bob_y_share_(y_share) {
    
    if (bob_y_share_ == 0) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(1, 1000000);
        bob_y_share_ = dis(gen);
    }
    
    std::cout << "Server starting on port " << port << std::endl;
    std::cout << "Bob's multiplicative share (y): " << bob_y_share_ << std::endl;
    
    start_accept();
}

void MTAServer::start_accept() {
    auto new_session = std::make_shared<Session>(io_context_, *mta_protocol_, *protobuf_handler_, bob_y_share_);
    acceptor_.async_accept(new_session->socket(),
        [this, new_session](boost::system::error_code ec) {
            if (!ec) {
                std::cout << "New client (Alice) connected" << std::endl;
                new_session->start();
            } else {
                std::cerr << "Accept error: " << ec.message() << std::endl;
            }
            start_accept();
        });
}

MTAServer::Session::Session(boost::asio::io_context& io_context, 
                           MTAProtocol& mta_protocol, 
                           MTAProtobufHandler& protobuf_handler,
                           uint32_t y_share)
    : socket_(io_context), 
      mta_protocol_(mta_protocol), 
      protobuf_handler_(protobuf_handler),
      bob_y_share_(y_share),
      state_(ProtocolState::WAITING_FOR_CORRELATION_DELTA),
      correlation_delta_(0),
      bob_additive_share_(0),
      bob_correlation_check_(0) {
    read_buffer_.resize(8192);
}

tcp::socket& MTAServer::Session::socket() {
    return socket_;
}

void MTAServer::Session::start() {
    std::cout << "Session started, waiting for correlation delta from Alice..." << std::endl;
    read_message_with_size();
}

void MTAServer::Session::read_message_with_size() {
    auto self(shared_from_this());

    boost::asio::async_read(socket_,
        boost::asio::buffer(read_buffer_, 4),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec && length == 4) {
                uint32_t message_size = read_buffer_[0] |
                                       (read_buffer_[1] << 8) |
                                       (read_buffer_[2] << 16) |
                                       (read_buffer_[3] << 24);

                std::cout << "Incoming message size: " << message_size << " bytes" << std::endl;

                last_message_size_ = message_size;

                if (message_size > read_buffer_.size()) {
                    read_buffer_.resize(message_size);
                }

                boost::asio::async_read(socket_,
                    boost::asio::buffer(read_buffer_, message_size),
                    [this, self, message_size](boost::system::error_code ec2, std::size_t length2) {
                        if (!ec2 && length2 == message_size) {
                            process_received_message();
                        } else {
                            std::cerr << "Error reading message content: " << ec2.message() << std::endl;
                        }
                    });
            } else {
                std::cerr << "Error reading message size: " << ec.message() << std::endl;
            }
        });
}

void MTAServer::Session::process_received_message() {
    std::cout << "[DEBUG] Current state: " << static_cast<int>(state_) << std::endl;
    std::cout << "[DEBUG] Processing message of size: " << last_message_size_ << " bytes\n";

    switch (state_) {
        case ProtocolState::WAITING_FOR_CORRELATION_DELTA:
            process_correlation_delta(std::vector<uint8_t>(
                read_buffer_.begin(),
                read_buffer_.begin() + last_message_size_));
            break;

        case ProtocolState::WAITING_FOR_ALICE_MESSAGES:
            process_alice_messages(std::vector<uint8_t>(
                read_buffer_.begin(),
                read_buffer_.begin() + last_message_size_));
            break;

        default:
            std::cerr << "Unexpected message received in state: " << static_cast<int>(state_) << std::endl;
            break;
    }
}

void MTAServer::Session::process_correlation_delta(const std::vector<uint8_t>& data) {
    uint32_t correlation_delta;

    std::cout << "[Debug] Raw CorrelationDelta bytes:";
    for (auto b : data) std::printf(" %02X", b);
    std::cout << std::endl;

    if (!protobuf_handler_.deserializeCorrelationDelta(data, correlation_delta)) {
        std::cerr << "Failed to deserialize correlation delta" << std::endl;
        return;
    }
    
    std::cout << "Received correlation delta: " << correlation_delta << std::endl;
    correlation_delta_ = correlation_delta;
    
    bob_setup_ = mta_protocol_.initializeAsBob(correlation_delta);
    if (!bob_setup_.success) {
        std::cerr << "Failed to initialize Bob setup" << std::endl;
        return;
    }

    bob_setup_.public_key.resize(65);
    for (size_t i = 0; i < 65; ++i) {
        bob_setup_.public_key[i] = static_cast<uint8_t>(i);
    }
    std::cout << "[INFO] Dummy public key injected (65 bytes)" << std::endl;
    
    std::cout << "Bob setup initialized successfully" << std::endl;
    std::cout << "Points B length: " << bob_setup_.points_B.size() << " bytes" << std::endl;
    
    state_ = ProtocolState::SENDING_BOB_SETUP;
    send_bob_setup();
}

void MTAServer::Session::process_alice_messages(const std::vector<uint8_t>& data) {
    MTAProtocol::AliceMessages alice_messages;
    std::cout << "[DEBUG] Raw AliceMessages buffer (" << data.size() << " bytes): ";
    for (size_t i = 0; i < std::min(data.size(), size_t(32)); ++i) {
        printf("%02X ", data[i]);
    }
    std::cout << (data.size() > 32 ? "... (truncated)" : "") << std::endl;
    
    if (!mta_protocol_.deserializeAliceMessages(data, alice_messages)) {
        std::cerr << "Failed to deserialize Alice messages" << std::endl;
        return;
    }

    std::cout << "Received Alice messages successfully" << std::endl;
    std::cout << "Success: " << alice_messages.success << std::endl;
    std::cout << "Alice's masked share: " << alice_messages.masked_share << std::endl;

    bob_messages_ = mta_protocol_.prepareBobMessages(bob_y_share_);
    if (!bob_messages_.success) {
        std::cerr << "Failed to prepare Bob messages" << std::endl;
        return;
    }

    auto mta_result = mta_protocol_.executeBobMTA(bob_y_share_, alice_messages);
    if (!mta_result.success) {
        std::cerr << "MTA protocol execution failed" << std::endl;
        return;
    }

    bob_additive_share_ = mta_result.additive_share;
    bob_correlation_check_ = (bob_y_share_ + bob_additive_share_) ^ correlation_delta_;

    std::cout << "\n=== MTA PROTOCOL COMPUTATION COMPLETED ===" << std::endl;
    std::cout << std::dec;
    std::cout << "Bob's Multiplicative Share: " << bob_y_share_ << std::endl;
    std::cout << "Bob's Additive Share: " << bob_additive_share_ << std::endl;
    std::cout << "Correlation Check Value: " << bob_correlation_check_ << std::endl;

    state_ = ProtocolState::SENDING_BOB_MESSAGES;
    send_bob_messages();
}

void MTAServer::Session::send_bob_messages() {
    if (!bob_messages_.success) {
        std::cerr << "Bob messages not ready!" << std::endl;
        return;
    }

    std::vector<uint8_t> serialized_messages = mta_protocol_.serializeBobMessages(bob_messages_);
    if (serialized_messages.empty()) {
        std::cerr << "Failed to serialize Bob messages" << std::endl;
        return;
    }

    std::cout << "Sending Bob messages (" << serialized_messages.size() << " bytes)" << std::endl;
    std::cout << "  - Masked share: " << bob_messages_.masked_share << std::endl;

    send_message_with_size(serialized_messages);
}

void MTAServer::Session::send_bob_setup() {
    protobuf_handler_.temp_ot_messages_ = mta_protocol_.splitIntoByteVectors(bob_setup_.points_B, 65);
    protobuf_handler_.temp_bytes_arrays_ = protobuf_handler_.temp_ot_messages_;

    mta_BobSetup proto_bob_setup = mta_BobSetup_init_zero;
    proto_bob_setup.success = bob_setup_.success;
    proto_bob_setup.num_ot_instances = bob_setup_.num_ot_instances;

    proto_bob_setup.ot_messages.funcs.encode = MTAProtobufHandler::encode_bytes_array;
    proto_bob_setup.ot_messages.arg = &protobuf_handler_.temp_bytes_arrays_;

    if (!bob_setup_.public_key.empty()) {
        proto_bob_setup.public_key.size = std::min((size_t)256, bob_setup_.public_key.size());
        std::copy(
            bob_setup_.public_key.begin(),
            bob_setup_.public_key.begin() + proto_bob_setup.public_key.size,
            proto_bob_setup.public_key.bytes
        );
    }

    std::cout << "\n=== Bob Setup Message ===" << std::endl;
    std::cout << "success: " << proto_bob_setup.success << std::endl;
    std::cout << "num_ot_instances: " << proto_bob_setup.num_ot_instances << std::endl;

    std::vector<uint8_t> serialized_setup = protobuf_handler_.serializeBobSetup(proto_bob_setup);
    if (serialized_setup.empty()) {
        std::cerr << "[ERROR] Failed to serialize Bob setup\n";
        return;
    }

    std::cout << "Sending Bob setup (" << serialized_setup.size() << " bytes)\n";
    state_ = ProtocolState::SENDING_BOB_SETUP;
    send_message_with_size(serialized_setup);
}

void MTAServer::Session::send_message_with_size(const std::vector<uint8_t>& message) {
    write_buffer_.clear();
    write_buffer_.resize(4 + message.size());
    uint32_t size = static_cast<uint32_t>(message.size());
    write_buffer_[0] = size & 0xFF;
    write_buffer_[1] = (size >> 8) & 0xFF;
    write_buffer_[2] = (size >> 16) & 0xFF;
    write_buffer_[3] = (size >> 24) & 0xFF;

    std::copy(message.begin(), message.end(), write_buffer_.begin() + 4);

    auto self(shared_from_this());
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_buffer_),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::cout << "Sent message (" << length << " bytes total)" << std::endl;

                if (state_ == ProtocolState::SENDING_BOB_SETUP) {
                    state_ = ProtocolState::WAITING_FOR_ALICE_MESSAGES;
                    std::cout << "Waiting for Alice's messages..." << std::endl;
                    read_message_with_size();
                } else if (state_ == ProtocolState::SENDING_BOB_MESSAGES) {
                    state_ = ProtocolState::PROTOCOL_COMPLETE;
                    std::cout << "Final Results:" << std::endl;
                    std::cout << std::dec;
                    std::cout << "Bob's Multiplicative Share: " << bob_y_share_ << std::endl;
                    std::cout << "Bob's Additive Share: " << bob_additive_share_ << std::endl;
                    std::cout << "Correlation Check: " << bob_correlation_check_ << std::endl;
                    std::cout << "Protocol executed successfully." << std::endl;
                }

            } else {
                std::cerr << "Error sending message: " << ec.message() << std::endl;
            }
        });
}