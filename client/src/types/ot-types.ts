export interface OTMessage {
    type: 'ot_request' | 'ot_response' | 'encrypted_messages';
    pointA?: Buffer;
    choiceBit?: number;
    pointB?: Buffer;
    encryptedM0?: Buffer;
    encryptedM1?: Buffer;
    messageLength?: number;
  }
  
  export interface ECPoint {
    x: Buffer;
    y: Buffer;
  }