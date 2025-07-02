#include "cot_protocol.h"
#include <iostream>
#include <cstring>

CorrelatedOTProtocol::CorrelatedOTProtocol() {
    ot_instances.reserve(BIT_LENGTH);
    stored_scalars.resize(BIT_LENGTH * 32);
    correlation_x = 0;
}

bool CorrelatedOTProtocol::getBit(uint32_t value, int bit_position) {
    return (value >> bit_position) & 1;
}

bool CorrelatedOTProtocol::generatePointB(int index, uint8_t* point_B_out) {
    if (index < 0 || index >= BIT_LENGTH) {
        return false;
    }
    
    uint8_t* b_scalar = &stored_scalars[index * 32];
    
    if (!crypto_ops.generateECDHKeyPair(b_scalar, point_B_out)) {
        std::cerr << "generateECDHKeyPair failed at index " << index << "\n";
        return false;
    }
    
    return true;
}

CorrelatedOTProtocol::COTSetup CorrelatedOTProtocol::initializeCOT(uint32_t alice_x) {
    COTSetup setup;
    setup.points_B.resize(BIT_LENGTH * 65);
    setup.correlation_x = alice_x;
    setup.success = false;
    
    correlation_x = alice_x;
    
    ot_instances.clear();
    
    for (int i = 0; i < BIT_LENGTH; i++) {
        ot_instances.push_back(std::make_unique<ObliviousTransferProtocol>());
        
        uint8_t* point_B = &setup.points_B[i * 65];
        if (!generatePointB(i, point_B)) {
            return setup;
        }
    }   

    setup.success = true;
    return setup;
}

bool CorrelatedOTProtocol::processSingleCOT(
    int bit_index,
    bool choice_bit,
    const uint8_t* point_A,
    const uint8_t* encrypted_m0,
    const uint8_t* encrypted_m1,
    size_t message_length,
    uint32_t& received_value
) {
    if (bit_index >= BIT_LENGTH || bit_index < 0) {
        return false;
    }
    uint8_t* b_scalar = &stored_scalars[bit_index * 32];
    
    uint8_t shared_secret[32];
    const uint8_t* encrypted_message = choice_bit ? encrypted_m1 : encrypted_m0;
    
    uint8_t decrypted_message[32];
    crypto_ops.xorEncryptDecrypt(encrypted_message, shared_secret, decrypted_message, message_length);
    
    received_value = crypto_ops.bytesToUint32(decrypted_message);
    
    return true;
}

CorrelatedOTProtocol::COTResult CorrelatedOTProtocol::executeCOTMultiplication(
    uint32_t y,
    const std::vector<uint8_t>& points_A,
    const std::vector<uint8_t>& encrypted_m0_messages,
    const std::vector<uint8_t>& encrypted_m1_messages
) {
    COTResult result = {0, false};
    
    if (points_A.size() != BIT_LENGTH * 65 ||
        encrypted_m0_messages.size() != BIT_LENGTH * 32 ||
        encrypted_m1_messages.size() != BIT_LENGTH * 32 ||
        ot_instances.size() != BIT_LENGTH) {
        return result;
    }
    uint32_t accumulated_V = 0;
    
    // Process each bit of y according to COT specification
    for (int i = 0; i < BIT_LENGTH; i++) {
        bool y_bit = getBit(y, i);  // yi = ith bit of y
        
        const uint8_t* point_A = &points_A[i * 65];
        const uint8_t* encrypted_m0 = &encrypted_m0_messages[i * 32];
        const uint8_t* encrypted_m1 = &encrypted_m1_messages[i * 32];
        
        uint32_t mc_i;  // this is Ui + yi * x
        if (!processSingleCOT(i, y_bit, point_A, encrypted_m0, encrypted_m1, 32, mc_i)) {
            return result;
        }
        
        // calculate V = Σ(2^i * mc_i)
        const uint64_t MODULUS = 0x100000000ULL;

        accumulated_V = (accumulated_V + ((uint64_t)mc_i * (1ULL << i)) % MODULUS) % MODULUS;

    }
    result.additive_share_V = accumulated_V;
    result.success = true;
    return result;
}

std::vector<uint8_t> CorrelatedOTProtocol::serializeCOTSetup(const COTSetup& setup) {
    std::vector<uint8_t> buffer;
    
    buffer.push_back(setup.success ? 1 : 0);
    
    uint32_t corr = setup.correlation_x;
    buffer.push_back(corr & 0xFF);
    buffer.push_back((corr >> 8) & 0xFF);
    buffer.push_back((corr >> 16) & 0xFF);
    buffer.push_back((corr >> 24) & 0xFF);
    
    buffer.insert(buffer.end(), setup.points_B.begin(), setup.points_B.end());
    
    return buffer;
}

bool CorrelatedOTProtocol::deserializeAliceMessages(
    const std::vector<uint8_t>& buffer, 
    AliceMessages& messages
) {
    if (buffer.size() < 1 + (BIT_LENGTH * 65) + (BIT_LENGTH * 32) + (BIT_LENGTH * 32)) {
        return false;
    }
    
    size_t offset = 0;
    
    messages.success = (buffer[offset++] == 1);
    
    messages.points_A.resize(BIT_LENGTH * 65);
    std::copy(buffer.begin() + offset, buffer.begin() + offset + (BIT_LENGTH * 65), 
              messages.points_A.begin());
    offset += BIT_LENGTH * 65;
    
    messages.encrypted_m0_messages.resize(BIT_LENGTH * 32);
    std::copy(buffer.begin() + offset, buffer.begin() + offset + (BIT_LENGTH * 32), 
              messages.encrypted_m0_messages.begin());
    offset += BIT_LENGTH * 32;
    
    messages.encrypted_m1_messages.resize(BIT_LENGTH * 32);
    std::copy(buffer.begin() + offset, buffer.begin() + offset + (BIT_LENGTH * 32), 
              messages.encrypted_m1_messages.begin());
    
    return true;
}

std::vector<uint8_t> CorrelatedOTProtocol::serializeCOTResult(const COTResult& result) {
    std::vector<uint8_t> buffer;
    
    buffer.push_back(result.success ? 1 : 0);
    
    //additive_share_V (4 bytes, little-endian)
    uint32_t share = result.additive_share_V;
    buffer.push_back(share & 0xFF);
    buffer.push_back((share >> 8) & 0xFF);
    buffer.push_back((share >> 16) & 0xFF);
    buffer.push_back((share >> 24) & 0xFF);
    
    return buffer;
}