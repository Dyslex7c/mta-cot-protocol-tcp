#ifndef CRYPTO_OPERATIONS_H
#define CRYPTO_OPERATIONS_H

#include <cstdint>
#include <vector>
#include "random_generator.h"

extern "C" {
    #include <trezor-crypto/bignum.h>
    #include <trezor-crypto/ecdsa.h>
    #include <trezor-crypto/secp256k1.h>
}

class CryptoOperations {
private:
    SecureRandom secure_random;
    
public:
    CryptoOperations();
    
    bool generateECDHKeyPair(uint8_t* private_key, uint8_t* public_point);
    bool generatePointFromScalar(const uint8_t* scalar, uint8_t* point_out);
    
    bool performECDH(const uint8_t* private_scalar, const uint8_t* public_point, uint8_t* shared_secret);
    
    void xorEncryptDecrypt(const uint8_t* data, const uint8_t* key, uint8_t* output, size_t length);
    
    bool validatePublicPoint(const uint8_t* point);
    
    uint32_t bytesToUint32(const uint8_t* bytes) const;
    void uint32ToBytes(uint32_t value, uint8_t* bytes) const;
    
    uint32_t generateRandomUint32();
    void generateRandomScalar(uint8_t* scalar_out);
};

#endif