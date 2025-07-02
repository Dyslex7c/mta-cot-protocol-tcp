#include <crypto_operations.h>
#include <cstring>
#include <iostream>

extern "C" {
    #include <trezor-crypto/rand.h>
}

CryptoOperations::CryptoOperations() {}

bool CryptoOperations::generateECDHKeyPair(uint8_t* private_key, uint8_t* public_point) {
    secure_random.generateScalar(private_key);
    
    return generatePointFromScalar(private_key, public_point);
}

bool CryptoOperations::generatePointFromScalar(const uint8_t* scalar, uint8_t* point_out) {
    bignum256 scalar_bn;
    bn_read_be(scalar, &scalar_bn);
    
    curve_point point;
    scalar_multiply(&secp256k1, &scalar_bn, &point);
    
    point_out[0] = 0x04;
    bn_write_be(&point.x, point_out + 1);
    bn_write_be(&point.y, point_out + 33);
    
    return true;
}

bool CryptoOperations::performECDH(const uint8_t* private_scalar, const uint8_t* public_point, uint8_t* shared_secret) {
    curve_point point;
    if (!ecdsa_read_pubkey(&secp256k1, public_point, &point)) {
        return false;
    }
    
    bignum256 scalar_bn;
    bn_read_be(private_scalar, &scalar_bn);
    
    curve_point result;
    point_multiply(&secp256k1, &scalar_bn, &point, &result);
    
    bn_write_be(&result.x, shared_secret);
    
    return true;
}

void CryptoOperations::xorEncryptDecrypt(const uint8_t* data, const uint8_t* key, uint8_t* output, size_t length) {
    for (size_t i = 0; i < length; i++) {
        output[i] = data[i] ^ key[i % 32];
    }
}

bool CryptoOperations::validatePublicPoint(const uint8_t* point) {
    curve_point parsed_point;
    return ecdsa_read_pubkey(&secp256k1, point, &parsed_point);
}

uint32_t CryptoOperations::bytesToUint32(const uint8_t* bytes) const {
    return secure_random.bytesToUint32(bytes);
}

void CryptoOperations::uint32ToBytes(uint32_t value, uint8_t* bytes) const {
    for (int i = 0; i < 4; i++) {
        bytes[i] = (value >> (8 * i)) & 0xFF;
    }
}

uint32_t CryptoOperations::generateRandomUint32() {
    return secure_random.generateMultiplicativeShare();
}

void CryptoOperations::generateRandomScalar(uint8_t* scalar_out) {
    secure_random.generateScalar(scalar_out);
}