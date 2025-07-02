#ifndef COT_PROTOCOL_H
#define COT_PROTOCOL_H

#include "ot_protocol.h"
#include "crypto_operations.h"
#include <vector>
#include <cstdint>
#include <memory>

class ObliviousTransferProtocol;
class CryptoOperations;

class CorrelatedOTProtocol {
private:
    static const int BIT_LENGTH = 32;
    std::vector<std::unique_ptr<ObliviousTransferProtocol>> ot_instances;
    std::vector<uint8_t> stored_scalars;
    CryptoOperations crypto_ops;

    uint32_t correlation_x;
    
    bool getBit(uint32_t value, int bit_position);
    bool generatePointB(int index, uint8_t* point_B_out);
    bool verifyScalarStorage();
public:
    CorrelatedOTProtocol();
    
    struct COTResult {
        uint32_t additive_share_V;
        bool success;
    };
    
    struct COTSetup {
        std::vector<uint8_t> points_B;
        uint32_t correlation_x;
        bool success;
    };
    
    struct AliceMessages {
        std::vector<uint8_t> points_A;
        std::vector<uint8_t> encrypted_m0_messages;
        std::vector<uint8_t> encrypted_m1_messages;
        bool success;
    };
    
    COTSetup initializeCOT(uint32_t alice_x);
    
    bool processSingleCOT(
        int bit_index,
        bool choice_bit,
        const uint8_t* point_A,
        const uint8_t* encrypted_m0,
        const uint8_t* encrypted_m1,
        size_t message_length,
        uint32_t& received_value
    );
    
    COTResult executeCOTMultiplication(
        uint32_t y,
        const std::vector<uint8_t>& points_A,
        const std::vector<uint8_t>& encrypted_m0_messages,
        const std::vector<uint8_t>& encrypted_m1_messages
    );
    
    std::vector<uint8_t> serializeCOTSetup(const COTSetup& setup);
    bool deserializeAliceMessages(const std::vector<uint8_t>& buffer, AliceMessages& messages);
    std::vector<uint8_t> serializeCOTResult(const COTResult& result);
};

#endif