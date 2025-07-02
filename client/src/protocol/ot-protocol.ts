import * as crypto from 'crypto';
import { ECCUtils } from '../crypto/utils.js';
import { EncryptionUtils } from '../crypto/encryption.js';
import { OTMessage } from '../types/ot-types.js';

/**
 * Alice's implementation of the Oblivious Transfer Protocol
 * compatible with Bob's C++ implementation using secp256k1
 */
export class OTProtocol {
  private a_scalar: Buffer;
  private readonly CURVE_NAME = 'secp256k1';

  constructor() {
    // Generate Alice's private scalar 'a'
    this.a_scalar = ECCUtils.generateScalar();
  }

  /**
   * Alice generates point A = aG and sends it to Bob
   */
  generatePointA(): Buffer {
    // A = a * G (scalar multiplication with generator point)
    return ECCUtils.scalarMultiplyG(this.a_scalar);
  } 

  /**
   * Alice receives point B from Bob and encrypts her two messages
   * Bob sends B = bG (if c=0) or B = bG + A (if c=1)
   * Alice encrypts m0 with key derived from a*B and m1 with key derived from a*(B-A)
   */
  encryptMessages(
    pointB: Buffer,
    message0: Buffer,
    message1: Buffer
  ): { encryptedM0: Buffer; encryptedM1: Buffer } {
    try {
      if (!ECCUtils.isValidPoint(pointB)) {
        throw new Error('Invalid point B received from Bob');
      }

      // Calculate aB (shared secret for message 0)
      const key0 = this.deriveKey0(pointB);
      
      // Calculate a(B-A) (shared secret for message 1)
      const key1 = this.deriveKey1(pointB);

      // Encrypt both messages
      const encryptedM0 = EncryptionUtils.encryptMessage(message0, key0);
      const encryptedM1 = EncryptionUtils.encryptMessage(message1, key1);

      return {
        encryptedM0,
        encryptedM1
      };
    } catch (error) {
      throw new Error(`Message encryption failed: ${error}`);
    }
  }

  /**
   * Derive key for message 0: key0 = x-coordinate of (a * B)
   * This matches Bob's calculation when c=0: Bob calculates b*A = b*(aG) = (ab)G
   * Alice calculates a*B = a*(bG) = (ab)G
   */
  private deriveKey0(pointB: Buffer): Buffer {
    // Calculate a * B
    const sharedPoint = this.scalarMultiplyPoint(this.a_scalar, pointB);
    
    // Extract x-coordinate as key (matching Bob's implementation)
    return EncryptionUtils.deriveKeyFromPoint(sharedPoint);
  }

  /**
   * Derive key for message 1: key1 = x-coordinate of (a * (B - A))
   * When Bob chooses c=1, he sends B = bG + A
   * So B - A = bG, and a*(B-A) = a*bG = (ab)G
   * Bob calculates b*A = (ab)G, so keys match
   */
  private deriveKey1(pointB: Buffer): Buffer {
    try {
      // Get point A (Alice's public point)
      const pointA = this.generatePointA();
      
      // Calculate B - A
      const pointB_minus_A = this.pointSubtract(pointB, pointA);
      
      // Calculate a * (B - A)
      const sharedPoint = this.scalarMultiplyPoint(this.a_scalar, pointB_minus_A);
      
      // Extract x-coordinate as key
      return EncryptionUtils.deriveKeyFromPoint(sharedPoint);
    } catch (error) {
      throw new Error(`Key derivation for message 1 failed: ${error}`);
    }
  }

  private scalarMultiplyPoint(scalar: Buffer, point: Buffer): Buffer {
    return ECCUtils.scalarMultiplyPoint(scalar, point);
  }  

  /**
   * Point subtraction: returns pointA - pointB
   * This is equivalent to pointA + (-pointB)
   */
  private pointSubtract(pointA: Buffer, pointB: Buffer): Buffer {
    // For secp256k1, we need to negate pointB and then add
    // Point negation: (x, y) -> (x, -y mod p)
    
    const negatedB = this.negatePoint(pointB);
    return ECCUtils.pointAdd(pointA, negatedB);
  }

  /**
   * Negate an elliptic curve point: (x, y) -> (x, -y mod p)
   */
  private negatePoint(point: Buffer): Buffer {
    if (point.length !== 65 || point[0] !== 0x04) {
      throw new Error('Invalid point format for negation');
    }

    const result = Buffer.alloc(65);
    result[0] = 0x04; // Uncompressed point prefix
    
    // Copy x-coordinate unchanged
    point.copy(result, 1, 1, 33);
    
    // Negate y-coordinate (mod p where p is the field prime for secp256k1)
    const y = point.subarray(33, 65);
    const negatedY = this.modNegate(y);
    negatedY.copy(result, 33);
    
    return result;
  }

  /**
   * Modular negation for secp256k1 field prime
   */
  private modNegate(value: Buffer): Buffer {
    // secp256k1 field prime p
    const p = Buffer.from('FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F', 'hex');
    
    // Calculate p - value
    const result = Buffer.alloc(32);
    let borrow = 0;
    
    for (let i = 31; i >= 0; i--) {
      let diff = p[i] - value[i] - borrow;
      if (diff < 0) {
        diff += 256;
        borrow = 1;
      } else {
        borrow = 0;
      }
      result[i] = diff;
    }
    
    return result;
  }

  async performObliviousTransfer(
    message0: Buffer,
    message1: Buffer
  ): Promise<{
    pointA: Buffer;
    getEncryptedMessages: (pointB: Buffer) => { encryptedM0: Buffer; encryptedM1: Buffer };
  }> {
    const pointA = this.generatePointA();
    
    return {
      pointA,
      getEncryptedMessages: (pointB: Buffer) => {
        return this.encryptMessages(pointB, message0, message1);
      }
    };
  }

  getPrivateScalar(): Buffer {
    return Buffer.from(this.a_scalar);
  }

  createOTRequestMessage(): OTMessage {
    return {
      type: 'ot_request',
      pointA: this.generatePointA()
    };
  }

  processOTResponse(
    response: OTMessage,
    message0: Buffer,
    message1: Buffer
  ): OTMessage {
    if (response.type !== 'ot_response' || !response.pointB) {
      throw new Error('Invalid OT response from Bob');
    }

    const { encryptedM0, encryptedM1 } = this.encryptMessages(
      response.pointB,
      message0,
      message1
    );

    return {
      type: 'encrypted_messages',
      encryptedM0,
      encryptedM1,
      messageLength: message0.length
    };
  }
}