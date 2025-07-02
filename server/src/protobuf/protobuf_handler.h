#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include "pb_encode.h"
#include "pb_decode.h"
#include "mta.pb.h"

class MTAProtobufHandler {
public:
    MTAProtobufHandler();
    ~MTAProtobufHandler();

    std::vector<uint8_t> serializeCorrelationDelta(uint32_t delta);
    std::vector<uint8_t> serializeBobSetup(const mta_BobSetup& setup);
    std::vector<uint8_t> serializeAliceMessages(const mta_AliceMessages& messages);
    std::vector<uint8_t> serializeBobMessages(const mta_BobMessages& messages);

        std::vector<std::vector<uint8_t>> temp_bytes_arrays_;
        std::vector<bool> temp_bool_array_;
        std::vector<uint8_t> temp_single_bytes_;
    
        std::vector<std::vector<uint8_t>> temp_ot_messages_;
        std::vector<std::vector<uint8_t>> temp_encrypted_shares_;
        std::vector<std::vector<uint8_t>> temp_ot_responses_;

    bool deserializeCorrelationDelta(const std::vector<uint8_t>& data, uint32_t& delta);
    bool deserializeBobSetup(const std::vector<uint8_t>& data, mta_BobSetup& setup);
    bool deserializeAliceMessages(const std::vector<uint8_t>& data, mta_AliceMessages& messages);
    bool deserializeBobMessages(const std::vector<uint8_t>& data, mta_BobMessages& messages);

    mta_BobSetup createBobSetup(bool success, 
                                const std::vector<std::vector<uint8_t>>& ot_messages,
                                const std::vector<uint8_t>& public_key,
                                uint32_t num_ot_instances);

    mta_AliceMessages createAliceMessages(uint32_t masked_share,
                                          const std::vector<bool>& ot_choices,
                                          const std::vector<std::vector<uint8_t>>& encrypted_shares);

    mta_BobMessages createBobMessages(bool success,
                                      const std::vector<std::vector<uint8_t>>& ot_responses,
                                      const std::vector<uint8_t>& encrypted_result,
                                      uint32_t correlation_check,
                                      uint32_t masked_share);

    static bool decode_single_bytes(pb_istream_t *stream, const pb_field_t *field, void **arg);
    static bool decode_bytes_array(pb_istream_t *stream, const pb_field_t *field, void **arg);
    static bool encode_bytes_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
    static bool decode_bool_array(pb_istream_t *stream, const pb_field_t *field, void **arg);
private:
    static bool encode_single_bytes(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
    static bool encode_bool_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
};