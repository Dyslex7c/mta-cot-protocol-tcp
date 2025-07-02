#include "mta_protocol.h"
#include "cot_protocol.h"
#include "crypto_operations.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include "protobuf_handler.h"

MTAProtocol::MTAProtocol() : beta(0) {
    cot_protocol = std::make_unique<CorrelatedOTProtocol>();
}

MTAProtocol::~MTAProtocol() = default;

uint32_t MTAProtocol::computeFinalShare(uint32_t received_share, uint32_t mask, uint32_t own_share) {
    return received_share + mask * own_share;
}

bool MTAProtocol::validateMTAInputs(uint32_t share1, uint32_t share2) {
    return true;
}

MTAProtocol::BobSetup MTAProtocol::initializeAsBob(uint32_t correlation_delta) {
    BobSetup setup;
    setup.success = true;
    setup.correlation_delta = correlation_delta;
    setup.num_ot_instances = 32;

    auto cot_setup = cot_protocol->initializeCOT(correlation_delta);
    if (!cot_setup.success) {
        std::cerr << "Failed to initialize COT protocol" << std::endl;
        return setup;
    }
    
    setup.points_B = std::move(cot_setup.points_B);
    setup.correlation_delta = correlation_delta;
    setup.success = true;
    
    std::cout << "Bob initialized COT with correlation delta: " << correlation_delta << std::endl;
    return setup;
}

MTAProtocol::BobMessages MTAProtocol::prepareBobMessages(uint32_t y_share) {
    BobMessages messages;
    messages.success = false;
    
    if (!validateMTAInputs(y_share, 0)) {
        std::cerr << "Invalid MTA inputs" << std::endl;
        return messages;
    }
    
    beta = crypto_ops.generateRandomUint32();
    
    messages.masked_share = y_share * beta;
    messages.success = true;
    
    std::cout << "Bob prepared messages with y_share: " << y_share 
              << ", beta: " << beta 
              << ", masked_share: " << messages.masked_share << std::endl;
    
    return messages;
}

MTAProtocol::MTAResult MTAProtocol::executeBobMTA(
    uint32_t y_share,
    const AliceMessages& alice_messages
) {
    MTAResult result;
    result.success = false;
    
    if (!alice_messages.success) {
        std::cerr << "Alice messages are invalid" << std::endl;
        return result;
    }
    
    if (!validateMTAInputs(y_share, alice_messages.masked_share)) {
        std::cerr << "Invalid MTA inputs for execution" << std::endl;
        return result;
    }
    
    std::cout << "Executing COT multiplication with y_share: " << y_share << std::endl;
    auto cot_result = cot_protocol->executeCOTMultiplication(
        y_share, 
        alice_messages.points_A, 
        alice_messages.encrypted_m0_messages, 
        alice_messages.encrypted_m1_messages
    );
    
    if (!cot_result.success) {
        std::cerr << "COT multiplication failed" << std::endl;
        return result;
    }
    
    std::cout << "COT result: " << cot_result.additive_share_V << std::endl;
    
    // share_B = beta * x_masked_share + cot_result
    const uint64_t MODULUS = 0x100000000ULL;
    uint64_t product = (uint64_t)beta * alice_messages.masked_share;
    uint64_t additive = (product + cot_result.additive_share_V) % MODULUS;
    result.additive_share = (uint32_t)additive;

    result.success = true;
    
    return result;
}

std::vector<uint8_t> MTAProtocol::serializeBobSetup(const BobSetup& setup) {
    std::cout << "[Bob] First point_B[0]: " << std::hex << (int)setup.points_B[0] << std::endl;

    static MTAProtobufHandler protobuf_handler;

    std::vector<std::vector<uint8_t>> ot_messages = splitIntoByteVectors(setup.points_B, 65);

    mta_BobSetup proto_setup = protobuf_handler.createBobSetup(
        setup.success,
        ot_messages,
        setup.public_key,
        setup.num_ot_instances
    );

    return protobuf_handler.serializeBobSetup(proto_setup);
}

std::vector<std::vector<uint8_t>> MTAProtocol::splitIntoByteVectors(const std::vector<uint8_t>& flat, size_t chunk_size) {
    std::vector<std::vector<uint8_t>> result;
    for (size_t i = 0; i < flat.size(); i += chunk_size) {
        result.emplace_back(flat.begin() + i, flat.begin() + std::min(i + chunk_size, flat.size()));
    }
    return result;
}

bool MTAProtocol::deserializeBobSetup(const std::vector<uint8_t>& buffer, BobSetup& setup) {
    static MTAProtobufHandler protobuf_handler;
    mta_BobSetup proto_setup = mta_BobSetup_init_zero;

    protobuf_handler.temp_bytes_arrays_.clear(); // Clear OT messages
    proto_setup.ot_messages.funcs.decode = MTAProtobufHandler::decode_bytes_array;
    proto_setup.ot_messages.arg = &protobuf_handler;

    pb_istream_t stream = pb_istream_from_buffer(buffer.data(), buffer.size());
    if (!pb_decode(&stream, mta_BobSetup_fields, &proto_setup)) {
        std::cerr << "[ERROR] Failed to decode mta_BobSetup: " << PB_GET_ERROR(&stream) << "\n";
        return false;
    }

    setup.success = proto_setup.success;
    setup.num_ot_instances = proto_setup.num_ot_instances;

    setup.points_B.clear();
    for (const auto& chunk : protobuf_handler.temp_bytes_arrays_) {
        setup.points_B.insert(setup.points_B.end(), chunk.begin(), chunk.end());
    }

    setup.public_key.clear();
    setup.public_key.insert(
        setup.public_key.end(),
        proto_setup.public_key.bytes,
        proto_setup.public_key.bytes + proto_setup.public_key.size
    );

    return true;
}

std::vector<uint8_t> MTAProtocol::serializeAliceMessages(const AliceMessages& messages) {
    std::vector<uint8_t> buffer;
    
    buffer.push_back(messages.success ? 1 : 0);
    
    uint32_t masked = messages.masked_share;
    buffer.push_back(masked & 0xFF);
    buffer.push_back((masked >> 8) & 0xFF);
    buffer.push_back((masked >> 16) & 0xFF);
    buffer.push_back((masked >> 24) & 0xFF);
    std::cout << "[SERIALIZE] First byte of points_A[0]: " << std::hex << (int)messages.points_A[0] << std::endl;
    buffer.insert(buffer.end(), messages.points_A.begin(), messages.points_A.end());
    
    buffer.insert(buffer.end(), messages.encrypted_m0_messages.begin(), messages.encrypted_m0_messages.end());
    
    buffer.insert(buffer.end(), messages.encrypted_m1_messages.begin(), messages.encrypted_m1_messages.end());
    
    return buffer;
}

bool MTAProtocol::deserializeAliceMessages(const std::vector<uint8_t>& buffer, AliceMessages& messages) {
    if (buffer.size() < 5) {
        return false;
    }
    
    size_t offset = 0;
    
    messages.success = (buffer[offset++] == 1);
    
    messages.masked_share = buffer[offset] | 
    (buffer[offset + 1] << 8) | 
    (buffer[offset + 2] << 16) | 
    (buffer[offset + 3] << 24);
    offset += 4;
    
    std::cout << "[DESERIALIZE] Parsing AliceMessages..." << std::endl;

    std::cout << "  > buffer size: " << buffer.size() << std::endl;
    const size_t points_size = 32 * 65;
    const size_t messages_size = 32 * 32;
    
    messages.points_A.resize(points_size);
    std::copy(buffer.begin() + offset, buffer.begin() + offset + points_size, 
              messages.points_A.begin());
    offset += points_size;
    std::cout << "[DESERIALIZE] First byte of points_A[0]: " << std::hex << (int)messages.points_A[0] << std::endl;
    
    messages.encrypted_m0_messages.resize(messages_size);
    std::copy(buffer.begin() + offset, buffer.begin() + offset + messages_size, 
              messages.encrypted_m0_messages.begin());
    offset += messages_size;
    
    messages.encrypted_m1_messages.resize(messages_size);
    std::copy(buffer.begin() + offset, buffer.begin() + offset + messages_size, 
              messages.encrypted_m1_messages.begin());
    messages.success = true;
    return true;
}

std::vector<uint8_t> MTAProtocol::serializeBobMessages(const BobMessages& messages) {
    MTAProtobufHandler protobuf_handler;

    std::vector<std::vector<uint8_t>> ot_responses = splitIntoByteVectors(messages.ot_responses, 32);
    std::vector<uint8_t> encrypted_result = messages.encrypted_result;
    mta_BobMessages proto_messages = protobuf_handler.createBobMessages(
        messages.success,
        ot_responses,
        encrypted_result,
        messages.correlation_check,
        messages.masked_share
    );

    return protobuf_handler.serializeBobMessages(proto_messages);
}

bool MTAProtocol::deserializeBobMessages(const std::vector<uint8_t>& buffer, BobMessages& messages) {
    if (buffer.size() != 5) {
        return false;
    }
    
    size_t offset = 0;
    
    messages.success = (buffer[offset++] == 1);
    
    messages.masked_share = buffer[offset] | 
                           (buffer[offset + 1] << 8) | 
                           (buffer[offset + 2] << 16) | 
                           (buffer[offset + 3] << 24);
    
    return true;
}

std::vector<uint8_t> MTAProtocol::serializeMTAResult(const MTAResult& result) {
    std::vector<uint8_t> buffer;
    
    buffer.push_back(result.success ? 1 : 0);
    
    uint32_t share = result.additive_share;
    buffer.push_back(share & 0xFF);
    buffer.push_back((share >> 8) & 0xFF);
    buffer.push_back((share >> 16) & 0xFF);
    buffer.push_back((share >> 24) & 0xFF);
    
    return buffer;
}

bool MTAProtocol::deserializeMTAResult(const std::vector<uint8_t>& buffer, MTAResult& result) {
    if (buffer.size() != 5) {
        return false;
    }
    
    size_t offset = 0;
    
    result.success = (buffer[offset++] == 1);
    
    result.additive_share = buffer[offset] | 
                           (buffer[offset + 1] << 8) | 
                           (buffer[offset + 2] << 16) | 
                           (buffer[offset + 3] << 24);
    
    return true;
}