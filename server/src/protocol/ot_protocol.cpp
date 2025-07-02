#include <ot_protocol.h>
#include <iostream>

extern "C" {
    #include <trezor-crypto/bignum.h>
}

using namespace std;

ObliviousTransferProtocol::ObliviousTransferProtocol() {
    rng.generateScalar(stored_b_scalar);
}

void ObliviousTransferProtocol::obliviousTransferWithStorage(
    const uint8_t* point_A, 
    int c, 
    uint8_t* point_B_out
) {
    if (c != 0 && c != 1) {
        std::cerr << "Error: Invalid choice bit" << std::endl;
        return;
    }
    
    uint8_t b[32];
    rng.generateScalar(b);
    storeScalar(b);
    
    curve_point A, B, bG;
    bignum256 b_bn;
    
    bn_read_be(b, &b_bn);
    
    if (!ecdsa_read_pubkey(&secp256k1, point_A, &A)) {
        std::cerr << "Error: Invalid point A" << std::endl;
        return;
    }
    
    scalar_multiply(&secp256k1, &b_bn, &bG);
    
    if (c == 0) {
        B = bG;
    } else {
        B = bG;
        point_add(&secp256k1, &A, &B);
    }
    
    point_B_out[0] = 0x04;
    bn_write_be(&B.x, point_B_out + 1);
    bn_write_be(&B.y, point_B_out + 33);
}

void ObliviousTransferProtocol::bobReceiveMessage(
    const uint8_t* point_A,
    int c,
    const uint8_t* encrypted_m0,
    const uint8_t* encrypted_m1,
    size_t message_length,
    uint8_t* decrypted_message_out
) {
    if (c != 0 && c != 1) {
        std::cerr << "Error: Invalid choice bit" << std::endl;
        return;
    }
    
    curve_point A;
    if (!ecdsa_read_pubkey(&secp256k1, point_A, &A)) {
        std::cerr << "Error: Invalid point A from Alice" << std::endl;
        return;
    }
    
    bignum256 b_bn;
    bn_read_be(stored_b_scalar, &b_bn);
    
    curve_point bA;
    point_multiply(&secp256k1, &b_bn, &A, &bA);
    
    uint8_t decryption_key[32];
    bn_write_be(&bA.x, decryption_key);
    
    const uint8_t* encrypted_message = (c == 0) ? encrypted_m0 : encrypted_m1;
    
    decryptMessage(encrypted_message, decryption_key, message_length, decrypted_message_out);
}

void ObliviousTransferProtocol::decryptMessage(
    const uint8_t* encrypted_message,
    const uint8_t* key,
    size_t message_length,
    uint8_t* decrypted_message_out
) {
    for (size_t i = 0; i < message_length; i++) {
        decrypted_message_out[i] = encrypted_message[i] ^ key[i % 32];
    }
}

void ObliviousTransferProtocol::storeScalar(const uint8_t* scalar) {
    memcpy(stored_b_scalar, scalar, 32);
}

void ObliviousTransferProtocol::getStoredScalar(uint8_t* scalar_out) {
    memcpy(scalar_out, stored_b_scalar, 32);
}