#ifndef OT_PROTOCOL_H
#define OT_PROTOCOL_H

#include <cstdint>
#include <array>
#include "crypto/random_generator.h"
#include <boost/asio.hpp>
#include <memory>
using namespace std;

extern "C" {
    #include <trezor-crypto/ecdsa.h>
    #include <trezor-crypto/secp256k1.h>
}

class ObliviousTransferProtocol {
    uint8_t stored_b_scalar[32];
    uint8_t b[32];
    SecureRandom rng;
    
    void decryptMessage(
        const uint8_t* encrypted_message,
        const uint8_t* key,
        size_t message_length,
        uint8_t* decrypted_message_out
    );
    
    void storeScalar(const uint8_t* scalar);
    
    public:
    void getStoredScalar(uint8_t* scalar_out);
    ObliviousTransferProtocol();
    void obliviousTransferWithStorage(const uint8_t* point_A, int c, uint8_t* point_B_out);
    void bobReceiveMessage(
        const uint8_t* point_A,
        int c,
        const uint8_t* encrypted_m0,
        const uint8_t* encrypted_m1,
        size_t message_length,
        uint8_t* decrypted_message_out
    );
};

#endif