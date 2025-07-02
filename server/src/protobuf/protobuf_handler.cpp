#include "protobuf_handler.h"
#include <iostream>

MTAProtobufHandler::MTAProtobufHandler() {}

MTAProtobufHandler::~MTAProtobufHandler() {}

std::vector<uint8_t> MTAProtobufHandler::serializeCorrelationDelta(uint32_t delta) {
    mta_CorrelationDelta msg = mta_CorrelationDelta_init_zero;
    msg.delta = delta;

    std::vector<uint8_t> buffer(128);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());

    if (!pb_encode(&stream, &mta_CorrelationDelta_msg, &msg)) {
        return std::vector<uint8_t>();
    }

    buffer.resize(stream.bytes_written);
    return buffer;
}

bool MTAProtobufHandler::deserializeCorrelationDelta(const std::vector<uint8_t>& data, uint32_t& delta) {
    mta_CorrelationDelta msg = mta_CorrelationDelta_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

    if (!pb_decode(&stream, &mta_CorrelationDelta_msg, &msg)) {
        std::cerr << "⚠️ Nanopb decode failed: " << PB_GET_ERROR(&stream) << std::endl;
        return false;
    }

    delta = msg.delta;
    return true;
}

std::vector<uint8_t> MTAProtobufHandler::serializeBobSetup(const mta_BobSetup& setup) {
    std::vector<uint8_t> buffer(4096);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());

    mta_BobSetup setup_copy = setup;

    if (!pb_encode(&stream, mta_BobSetup_fields, &setup_copy)) {
        std::cerr << "[ERROR] Failed to encode mta_BobSetup: " << PB_GET_ERROR(&stream) << "\n";
        return {};
    }

    buffer.resize(stream.bytes_written);
    return buffer;
}

bool MTAProtobufHandler::deserializeBobSetup(const std::vector<uint8_t>& data, mta_BobSetup& setup) {
    setup = mta_BobSetup_init_zero;

    temp_bytes_arrays_.clear();
    setup.ot_messages.funcs.decode = decode_bytes_array;
    setup.ot_messages.arg = this;

    pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

    if (!pb_decode(&stream, mta_BobSetup_fields, &setup)) {
        std::cerr << "[ERROR] Failed to decode mta_BobSetup: " << PB_GET_ERROR(&stream) << "\n";
        return false;
    }

    return true;
}

std::vector<uint8_t> MTAProtobufHandler::serializeAliceMessages(const mta_AliceMessages& messages) {
    std::vector<uint8_t> buffer(4096);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());

    if (!pb_encode(&stream, &mta_AliceMessages_msg, &messages)) {
        return std::vector<uint8_t>();
    }

    buffer.resize(stream.bytes_written);
    return buffer;
}

bool MTAProtobufHandler::deserializeAliceMessages(const std::vector<uint8_t>& data, mta_AliceMessages& messages) {
    messages = mta_AliceMessages_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());
    
    temp_bytes_arrays_.clear();
    temp_bool_array_.clear();
    
    messages.ot_choices.funcs.decode = decode_bool_array;
    messages.ot_choices.arg = this;
    messages.encrypted_shares.funcs.decode = decode_bytes_array;
    messages.encrypted_shares.arg = this;

    if (!pb_decode(&stream, &mta_AliceMessages_msg, &messages)) {
        return false;
    }

    return true;
}

std::vector<uint8_t> MTAProtobufHandler::serializeBobMessages(const mta_BobMessages& messages) {
    std::vector<uint8_t> buffer(4096);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());

    if (!pb_encode(&stream, &mta_BobMessages_msg, &messages)) {
        return std::vector<uint8_t>();
    }

    buffer.resize(stream.bytes_written);
    return buffer;
}

bool MTAProtobufHandler::deserializeBobMessages(const std::vector<uint8_t>& data, mta_BobMessages& messages) {
    messages = mta_BobMessages_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

    temp_bytes_arrays_.clear();

    messages.ot_responses.funcs.decode = decode_bytes_array;
    messages.ot_responses.arg = this;

    if (!pb_decode(&stream, &mta_BobMessages_msg, &messages)) {
        std::cerr << "[PROTOBUF ERROR] Failed to decode BobMessages: " << PB_GET_ERROR(&stream) << std::endl;
        return false;
    }

    return true;
}

mta_BobSetup MTAProtobufHandler::createBobSetup(
    bool success,
    const std::vector<std::vector<uint8_t>>& ot_messages,
    const std::vector<uint8_t>& public_key,
    uint32_t num_ot_instances
) {
    mta_BobSetup setup = mta_BobSetup_init_zero;

    setup.success = success;
    setup.num_ot_instances = num_ot_instances;

    this->temp_bytes_arrays_ = ot_messages;
    setup.ot_messages.funcs.encode = encode_bytes_array;
    setup.ot_messages.arg = this;

    setup.public_key.size = std::min((size_t)256, public_key.size());
    std::copy(public_key.begin(), public_key.begin() + setup.public_key.size, setup.public_key.bytes);

    return setup;
}

mta_AliceMessages MTAProtobufHandler::createAliceMessages(uint32_t masked_share,
    const std::vector<bool>& ot_choices,
    const std::vector<std::vector<uint8_t>>& encrypted_shares) {
mta_AliceMessages messages = mta_AliceMessages_init_zero;
messages.masked_share = masked_share;

temp_bool_array_ = ot_choices;
messages.ot_choices.funcs.encode = encode_bool_array;
messages.ot_choices.arg = &temp_bool_array_;

temp_encrypted_shares_ = encrypted_shares;
messages.encrypted_shares.funcs.encode = encode_bytes_array;
messages.encrypted_shares.arg = &temp_encrypted_shares_;

return messages;
}

mta_BobMessages MTAProtobufHandler::createBobMessages(
    bool success,
    const std::vector<std::vector<uint8_t>>& ot_responses,
    const std::vector<uint8_t>& encrypted_result,
    uint32_t correlation_check,
    uint32_t masked_share
) {
    mta_BobMessages messages = mta_BobMessages_init_zero;
    messages.success = success;
    messages.correlation_check = correlation_check;
    messages.masked_share = masked_share;

    temp_ot_responses_ = ot_responses;
    messages.ot_responses.funcs.encode = encode_bytes_array;
    messages.ot_responses.arg = &temp_ot_responses_;

    if (encrypted_result.size() > sizeof(messages.encrypted_result.bytes)) {
        std::cerr << "[ERROR] Encrypted result exceeds max allowed size\n";
    } else {
        messages.encrypted_result.size = encrypted_result.size();
        std::memcpy(messages.encrypted_result.bytes, encrypted_result.data(), encrypted_result.size());
    }

    return messages;
}

bool MTAProtobufHandler::encode_bytes_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    auto* array = reinterpret_cast<const std::vector<std::vector<uint8_t>>*>(*arg);
    for (const auto& item : *array) {
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        if (!pb_encode_string(stream, item.data(), item.size()))
            return false;
    }
    return true;
}

bool MTAProtobufHandler::decode_bytes_array(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    MTAProtobufHandler* handler = static_cast<MTAProtobufHandler*>(*arg);

    pb_istream_t substream;
    if (!pb_make_string_substream(stream, &substream)) {
        std::cerr << "[PROTOBUF ERROR] Failed to make string substream\n";
        return false;
    }

    std::vector<uint8_t> bytes(substream.bytes_left);
    if (!pb_read(&substream, bytes.data(), substream.bytes_left)) {
        std::cerr << "[PROTOBUF ERROR] Failed to read from substream\n";
        pb_close_string_substream(stream, &substream);
        return false;
    }

    handler->temp_bytes_arrays_.push_back(std::move(bytes));

    if (!pb_close_string_substream(stream, &substream)) {
        std::cerr << "[PROTOBUF ERROR] Failed to close string substream\n";
        return false;
    }

    return true;
}

bool MTAProtobufHandler::encode_single_bytes(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    MTAProtobufHandler* handler = static_cast<MTAProtobufHandler*>(*arg);
    
    if (!pb_encode_string(stream, handler->temp_single_bytes_.data(), handler->temp_single_bytes_.size())) {
        return false;
    }
    
    return true;
}

bool MTAProtobufHandler::decode_single_bytes(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    MTAProtobufHandler* handler = static_cast<MTAProtobufHandler*>(*arg);
    
    handler->temp_single_bytes_.resize(stream->bytes_left);
    
    if (!pb_read(stream, handler->temp_single_bytes_.data(), stream->bytes_left)) {
        return false;
    }
    
    return true;
}

bool MTAProtobufHandler::encode_bool_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    MTAProtobufHandler* handler = static_cast<MTAProtobufHandler*>(*arg);
    
    for (bool choice : handler->temp_bool_array_) {
        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }
        
        if (!pb_encode_varint(stream, choice ? 1 : 0)) {
            return false;
        }
    }
    
    return true;
}

bool MTAProtobufHandler::decode_bool_array(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    MTAProtobufHandler* handler = static_cast<MTAProtobufHandler*>(*arg);
    
    uint64_t value;
    if (!pb_decode_varint(stream, &value)) {
        return false;
    }
    
    handler->temp_bool_array_.push_back(value != 0);
    return true;
}