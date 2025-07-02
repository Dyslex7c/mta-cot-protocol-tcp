cmake_minimum_required(VERSION 3.10)
project(mta_cot_protocol C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

# Boost
find_package(Boost REQUIRED COMPONENTS system)
include_directories(SYSTEM /opt/homebrew/opt/boost/include)

# ---------- Trezor Crypto ----------
set(TREZOR_CRYPTO_SOURCES
    external/trezor-crypto/address.c
    external/trezor-crypto/base58.c
    external/trezor-crypto/bignum.c
    external/trezor-crypto/curves.c
    external/trezor-crypto/ecdsa.c
    external/trezor-crypto/hmac.c
    external/trezor-crypto/memzero.c
    external/trezor-crypto/rand.c
    external/trezor-crypto/rfc6979.c
    external/trezor-crypto/secp256k1.c
    external/trezor-crypto/sha2.c
    external/trezor-crypto/hasher.c
    external/trezor-crypto/blake256.c
    external/trezor-crypto/blake2b.c
    external/trezor-crypto/groestl.c
    external/trezor-crypto/sha3.c
    external/trezor-crypto/ripemd160.c
    external/trezor-crypto/hmac_drbg.c
)

add_library(trezor_crypto STATIC ${TREZOR_CRYPTO_SOURCES})
target_include_directories(trezor_crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/trezor-crypto
)

# ---------- NanoPB ----------
set(NANOPB_SOURCES
    external/nanopb/pb_common.c
    external/nanopb/pb_decode.c
    external/nanopb/pb_encode.c
)

add_library(nanopb STATIC ${NANOPB_SOURCES})
target_include_directories(nanopb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nanopb
)

# ---------- Secure Random ----------
add_library(secure_random STATIC
    src/crypto/random_generator.cpp
)
target_include_directories(secure_random PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)
target_link_libraries(secure_random PRIVATE trezor_crypto)

# ---------- Crypto Operations ----------
add_library(crypto_ops STATIC
    src/crypto/crypto_operations.cpp
)
target_include_directories(crypto_ops PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/external/trezor-crypto
)
target_link_libraries(crypto_ops PRIVATE secure_random trezor_crypto)

# ---------- OT + COT ----------
add_library(cot STATIC
    src/protocol/cot_protocol.cpp
    src/protocol/ot_protocol.cpp
)
target_include_directories(cot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)
target_link_libraries(cot PRIVATE secure_random crypto_ops)

# ---------- MTA Protocol ----------
add_library(mta_protocol STATIC
    src/protocol/mta_protocol.cpp
)
target_include_directories(mta_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protobuf
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nanopb
)
target_link_libraries(mta_protocol PRIVATE secure_random crypto_ops cot)

# ---------- Protobuf Handler ----------
add_library(protobuf_handler STATIC
    src/protobuf/protobuf_handler.cpp
    src/protobuf/mta.pb.c       # ✅ ADD THIS FILE TO FIX LINK ERRORS
)
target_include_directories(protobuf_handler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protobuf
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nanopb
)
target_link_libraries(protobuf_handler PRIVATE nanopb)

# ---------- MTA Server ----------
add_library(mta_server STATIC
    src/tcp/mta_server.cpp
)
target_include_directories(mta_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protobuf
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nanopb
)
target_link_libraries(mta_server PRIVATE
    mta_protocol
    protobuf_handler
    Boost::system
)

# ---------- Main Executable ----------
add_executable(tcp_server src/main.cpp)
target_include_directories(tcp_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)
target_link_libraries(tcp_server PRIVATE
    mta_server
    secure_random
    trezor_crypto
    nanopb
    Boost::system
    pthread
)
