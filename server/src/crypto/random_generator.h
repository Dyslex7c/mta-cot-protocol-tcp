#ifndef RANDOM_GENERATOR_H
#define RANDOM_GENERATOR_H

#include <cstdint>
#include <stdexcept>

extern "C" {
    #include "trezor-crypto/rand.h"
    #include "trezor-crypto/secp256k1.h"
    #include "trezor-crypto/ecdsa.h"
    #include "trezor-crypto/bignum.h"
}

class SecureRandom {
    
public:
    uint32_t bytesToUint32(const uint8_t* bytes) const;
    SecureRandom();
    uint32_t generateMultiplicativeShare();
    void generateScalar(uint8_t* out);
};

#endif
