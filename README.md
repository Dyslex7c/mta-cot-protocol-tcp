# MTA Protocol over TCP with COT

This project implements a secure client-server architecture that performs Multiplicative-to-Additive (MTA) share conversion using the Correlated Oblivious Transfer (COT) protocol, following the exact steps outlined in Appendix A.3.1, A.3.2, and A.3.3 of [COT.pdf](https://drive.google.com/file/d/1vxocILe5d3aUqj0qAwWZGzggCacQC4Q7/view). The server is written in C++ using `boost.asio` for networking, `trezor-crypto` for all cryptographic operations, and `nanopb` for Protocol Buffers. The client is written in TypeScript using Node.js, with the crypto module for secure randomness, net for TCP communication, and protobufjs for message serialization. Both sides generate 32-byte random values as multiplicative shares, convert them to additive shares using the MTA protocol, and print the final results.

I followed the algorithms closely, using secp256k1 for all elliptic curve operations and handling all intermediate messages, encodings, and randomness securely and precisely.CMake is used for the server build, and the client runs via Node.js after TypeScript compilation. This setup demonstrates a complete cryptographic interaction pipeline between two independent parties over a secure TCP connection. There might be some issues here and there given the fact that this project was built in a short period of time, but the core logic is implemented correctly.

## Build & Run Instructions
### Server (C++)
Ensure boost, trezor-crypto, nanopb, and cmake are installed.

Navigate to the `server/` directory.

Run:
```bash
mkdir build && cd build
cmake ..
make
./tcp_server
```

OR simply run
```bash
chmod +x ./run.sh
./run.sh
cd build
./tcp_server
```

### Client (Node.js + TypeScript)

Navigate to the `client/` directory.

Run:

```bash
npm install
npm run build
npm run start
```

### Here is a screenshot with random inputs
![alt text](<image.png>)