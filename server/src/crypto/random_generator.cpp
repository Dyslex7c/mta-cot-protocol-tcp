#include <random_generator.h>

extern "C" {
    #include <trezor-crypto/rand.h>
}

SecureRandom::SecureRandom() {}

uint32_t SecureRandom::bytesToUint32(const uint8_t* bytes) const {
    uint32_t value = 0;
    for (int i = 3; i >= 0; --i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

uint32_t SecureRandom::generateMultiplicativeShare() {
    return random32();
}

void SecureRandom::generateScalar(uint8_t* out) {
    uint8_t candidate[32];
    
    do {
        random_buffer(candidate, 32);
        
        bignum256 bn_candidate;
        bn_read_be(candidate, &bn_candidate);
        
        if (!bn_is_zero(&bn_candidate) && 
            bn_is_less(&bn_candidate, &secp256k1.order)) {
            break;
        }
    } while (1);
    
    for (int i = 0; i < 32; i++) {
        out[i] = candidate[i];
    }
}