import * as crypto from 'crypto';

export class EncryptionUtils {
  /**
   * Encrypt message using XOR with key (same as Bob's implementation)
   */
  static encryptMessage(message: Buffer, key: Buffer): Buffer {
    const encrypted = Buffer.alloc(message.length);
    for (let i = 0; i < message.length; i++) {
      encrypted[i] = message[i] ^ key[i % 32];
    }
    return encrypted;
  }

  /**
   * Decrypt message using XOR with key
   */
  static decryptMessage(encryptedMessage: Buffer, key: Buffer): Buffer {
    return this.encryptMessage(encryptedMessage, key); // XOR is symmetric
  }

  /**
   * Derive encryption key from EC point
   */
  static deriveKeyFromPoint(point: Buffer): Buffer {
    if (point.length === 65) {
      return point.subarray(1, 33);
    } else if (point.length === 32) {
      return point;
    } else {
      throw new Error('Invalid point format for key derivation');
    }
  }

  static generateRandomKey(): Buffer {
    return crypto.randomBytes(32);
  }
}