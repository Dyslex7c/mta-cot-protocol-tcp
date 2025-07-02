import * as crypto from 'crypto';
import { ec as EC } from 'elliptic';

const secp256k1 = new EC('secp256k1');

export class ECCUtils {
  /**
   * generate a random private scalar (32 byte Buffer)
   */
  static generateScalar(): Buffer {
    const ecdh = crypto.createECDH('secp256k1');
    ecdh.generateKeys();
    return ecdh.getPrivateKey();
  }

  /**
   * Compute a*G using crypto (public key from scalar)
   */
  static scalarMultiplyG(scalar: Buffer): Buffer {
    const ecdh = crypto.createECDH('secp256k1');
    ecdh.setPrivateKey(scalar);
    return ecdh.getPublicKey(null, 'uncompressed'); // 65-byte SEC1 format
  }

  /**
   * Compute a*B using ECDH-style scalar multiply (shared secret)
   * Returns x-coordinate (not a full EC point)
   */
  static scalarMultiplyPoint(scalar: Buffer, point: Buffer): Buffer {
    const ecdh = crypto.createECDH('secp256k1');
    ecdh.setPrivateKey(scalar);

    return ecdh.computeSecret(point);
  }

  /**
   * add two elliptic curve points using elliptic lib
   */
  static pointAdd(pointA: Buffer, pointB: Buffer): Buffer {
    const A = secp256k1.curve.decodePoint(pointA);
    const B = secp256k1.curve.decodePoint(pointB);
    const result = A.add(B);
    return Buffer.from(result.encode('array', false));
  }

  /**
   * Subtract B from A: A - B = A + (-B)
   */
  static pointSubtract(pointA: Buffer, pointB: Buffer): Buffer {
    const A = secp256k1.curve.decodePoint(pointA);
    const B = secp256k1.curve.decodePoint(pointB).neg();
    const result = A.add(B);
    return Buffer.from(result.encode('array', false));
  }

  /**
   * Negate a point: (x, y) → (x, -y mod p)
   */
  static negatePoint(point: Buffer): Buffer {
    const P = secp256k1.curve.decodePoint(point);
    const negated = P.neg();
    return Buffer.from(negated.encode('array', false));
  }

  /**
   * validate if a buffer is a valid point on the curve
   */
  static isValidPoint(point: Buffer): boolean {
    try {
      const decoded = secp256k1.curve.decodePoint(point);
      return decoded.validate();
    } catch {
      return false;
    }
  }
}
