#ifndef MTA_PROTOCOL_H
#define MTA_PROTOCOL_H

#include "crypto_operations.h"
#include "cot_protocol.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <string>

// Forward declarations
class CorrelatedOTProtocol;
class CryptoOperations;

class MTAProtocol {
private:
    std::unique_ptr<CorrelatedOTProtocol> cot_protocol;
    CryptoOperations crypto_ops;
    
    // Bob's random mask
    uint32_t beta;
    
    // Helper methods
    uint32_t computeFinalShare(uint32_t received_share, uint32_t mask, uint32_t own_share);
    
public:
    MTAProtocol();
    ~MTAProtocol();
    
    // Result structures for Bob (server)
    struct MTAResult {
        uint32_t additive_share;
        bool success;
        std::string error_message; // For error reporting
        
        MTAResult() : additive_share(0), success(false) {}
    };
    
    struct BobSetup {
        std::vector<uint8_t> points_B;
        uint32_t correlation_delta;
        bool success;
        uint32_t num_ot_instances;
        std::vector<uint8_t> public_key;
        
        BobSetup() : correlation_delta(0), success(false) {}
    };
    
    struct AliceMessages {
        std::vector<uint8_t> points_A;
        std::vector<uint8_t> encrypted_m0_messages;
        std::vector<uint8_t> encrypted_m1_messages;
        uint32_t masked_share;  // x * alpha
        bool success;
        
        AliceMessages() : masked_share(0), success(false) {}
    };
    
    struct BobMessages {
        uint32_t masked_share;  // y * beta
        bool success;
    
        // Required for Protobuf serialization
        std::vector<uint8_t> ot_responses;
        std::vector<uint8_t> encrypted_result;
        uint32_t correlation_check;
    
        BobMessages() 
            : masked_share(0), success(false), correlation_check(0) {}
    };    
    
    // Bob's server methods
    BobSetup initializeAsBob(uint32_t correlation_delta);
    BobMessages prepareBobMessages(uint32_t y_share);
    MTAResult executeBobMTA(
        uint32_t y_share,
        const AliceMessages& alice_messages
    );
    
    std::vector<std::vector<uint8_t>> splitIntoByteVectors(const std::vector<uint8_t>& flat, size_t chunk_size);

    // Utility methods
    bool validateMTAInputs(uint32_t share1, uint32_t share2);
    
    // Serialization methods for TCP communication
    std::vector<uint8_t> serializeBobSetup(const BobSetup& setup);
    bool deserializeBobSetup(const std::vector<uint8_t>& buffer, BobSetup& setup);
    
    std::vector<uint8_t> serializeAliceMessages(const AliceMessages& messages);
    bool deserializeAliceMessages(const std::vector<uint8_t>& buffer, AliceMessages& messages);
    
    std::vector<uint8_t> serializeBobMessages(const BobMessages& messages);
    bool deserializeBobMessages(const std::vector<uint8_t>& buffer, BobMessages& messages);
    
    std::vector<uint8_t> serializeMTAResult(const MTAResult& result);
    bool deserializeMTAResult(const std::vector<uint8_t>& buffer, MTAResult& result);
};

#endif