import { OTProtocol } from './ot-protocol.js';
import * as crypto from 'crypto'
/**
 * Alice's implementation of the Correlated Oblivious Transfer Protocol
 * compatible with Bob's C++ COT implementation for secure multiplication
 */
export class AliceCOTProtocol {
  private readonly BIT_LENGTH = 32;
  
  // Alice's private input x and random values
  private x: number = 0;
  private a_scalars: Buffer[] = [];
  private random_U: number[] = [];
  private ot_instances: OTProtocol[] = [];

  constructor() {
    this.a_scalars = [];
    this.random_U = [];
    this.ot_instances = [];
  }

  /**
   * initialize COT protocol with Alice's multiplicative input x
   */
  initializeCOT(x: number): COTSetup {
    console.log('[DEBUG] Initializing COT protocol with x =', x);

    this.x = x;
    this.a_scalars = [];
    this.random_U = [];
    this.ot_instances = [];

    // Generate OT instances and random values for each bit position
    for (let i = 0; i < this.BIT_LENGTH; i++) {
      this.ot_instances.push(new OTProtocol());
      this.random_U.push(this.generateRandomUint32());
    }

    const setup: COTSetup = {
      points_A: this.generatePointsA(),
      correlation_x: x,
      success: true
    };

    return setup;
  }
  
  generateRandomUint32(): number {
    return crypto.randomBytes(4).readUInt32LE();
  }  

  /**
   * Generate points A for all bit positions: Ai = ai * G
   */
  private generatePointsA(): Buffer {
    const pointsBuffer = Buffer.alloc(this.BIT_LENGTH * 65);
    
    for (let i = 0; i < this.BIT_LENGTH; i++) {
      const pointA = this.ot_instances[i].generatePointA();
      pointA.copy(pointsBuffer, i * 65);
    }
    
    return pointsBuffer;
  }

  processCOTSetup(bobSetup: COTSetup): AliceMessages {
    if (this.ot_instances.length !== this.BIT_LENGTH) {
      throw new Error(
        'COT protocol not initialized: call initializeCOT(x) before processCOTSetup()'
      );
    }
    
    if (!bobSetup.success || bobSetup.points_B.length !== this.BIT_LENGTH * 65) {
      return {
        points_A: Buffer.alloc(0),
        encrypted_m0_messages: Buffer.alloc(0),
        encrypted_m1_messages: Buffer.alloc(0),
        success: false
      };
    }

    const pointsA = this.generatePointsA();
    const encryptedM0Messages = Buffer.alloc(this.BIT_LENGTH * 32);
    const encryptedM1Messages = Buffer.alloc(this.BIT_LENGTH * 32);

    for (let i = 0; i < this.BIT_LENGTH; i++) {
      const pointB = bobSetup.points_B.subarray(i * 65, (i + 1) * 65);
    
      const { m0, m1 } = this.generateCOTMessages(i);
    
      try {
        const { encryptedM0, encryptedM1 } = this.ot_instances[i].encryptMessages(pointB, m0, m1);
    
        const paddedM0 = Buffer.alloc(32);
        const paddedM1 = Buffer.alloc(32);
        encryptedM0.copy(paddedM0, 0, 0, Math.min(encryptedM0.length, 32));
        encryptedM1.copy(paddedM1, 0, 0, Math.min(encryptedM1.length, 32));
    
        paddedM0.copy(encryptedM0Messages, i * 32);
        paddedM1.copy(encryptedM1Messages, i * 32);
      } catch (err) {
        console.error(`[ERROR] OT message encryption failed at index ${i}:`, err);
        console.error(`Corrupted point B: ${pointB.toString('hex')}`);
        return {
          points_A: Buffer.alloc(0),
          encrypted_m0_messages: Buffer.alloc(0),
          encrypted_m1_messages: Buffer.alloc(0),
          success: false
        };
      }
    }    

    return {
      points_A: pointsA,
      encrypted_m0_messages: encryptedM0Messages,
      encrypted_m1_messages: encryptedM1Messages,
      success: true
    };
  }

  /**
   * Generate the two COT messages for bit position i
   * m0 = Ui (random value when Bob's bit is 0)
   * m1 = Ui + x (correlated value when Bob's bit is 1)
   */
  private generateCOTMessages(bitIndex: number): { m0: Buffer; m1: Buffer } {
    const U_i = this.random_U[bitIndex];
    const correlatedValue = (U_i + this.x) >>> 0; // Ensure 32-bit unsigned

    const m0 = Buffer.alloc(32);
    const m1 = Buffer.alloc(32);

    // Convert to little-endian bytes (matching Bob's implementation)
    this.uint32ToBytes(U_i, m0);
    this.uint32ToBytes(correlatedValue, m1);

    return { m0, m1 };
  }

  /**
   * Convert 32-bit unsigned integer to little-endian bytes
   */
  private uint32ToBytes(value: number, buffer: Buffer): void {
    buffer.fill(0);
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
  }

  /**
   * Calculate Alice's additive share of the multiplication result
   * Alice's share U = Σ(2^i * Ui)
   */
  calculateAdditiveShare(): number {
    let U = 0;
    const MOD = 2 ** 32;
  
    for (let i = 0; i < this.BIT_LENGTH; i++) {
      const term = (this.random_U[i] * (1 << i)) >>> 0;
      U = (U + term) % MOD;
    }
  
    return U >>> 0;
  }
  
  async performCOTMultiplication(
    x: number,
    bobSetup: COTSetup
  ): Promise<COTResult> {
    try {
      this.initializeCOT(x);
      
      const aliceMessages = this.processCOTSetup(bobSetup);
      
      if (!aliceMessages.success) {
        return { additive_share_U: 0, success: false };
      }
      
      const additiveShare = this.calculateAdditiveShare();
      
      return {
        additive_share_U: additiveShare,
        success: true,
        alice_messages: aliceMessages
      };
    } catch (error) {
      console.error('COT multiplication failed:', error);
      return { additive_share_U: 0, success: false };
    }
  }

  serializeCOTSetup(setup: COTSetup): Buffer {
    const buffer = Buffer.alloc(1 + 4 + setup.points_A.length);
    let offset = 0;
    
    buffer[offset++] = setup.success ? 1 : 0;
    
    // correlation_x (4 bytes, little-endian)
    buffer.writeUInt32LE(setup.correlation_x, offset);
    offset += 4;
    
    setup.points_A.copy(buffer, offset);
    
    return buffer;
  }

  /**
   * Serialize Alice's messages for transmission to Bob
   */
  serializeAliceMessages(messages: AliceMessages): Buffer {
    const totalSize = 1 + messages.points_A.length + 
                     messages.encrypted_m0_messages.length + 
                     messages.encrypted_m1_messages.length;
    
    const buffer = Buffer.alloc(totalSize);
    let offset = 0;
    
    buffer[offset++] = messages.success ? 1 : 0;
    
    messages.points_A.copy(buffer, offset);
    offset += messages.points_A.length;
    
    messages.encrypted_m0_messages.copy(buffer, offset);
    offset += messages.encrypted_m0_messages.length;
    
    messages.encrypted_m1_messages.copy(buffer, offset);
    
    return buffer;
  }

  deserializeCOTSetup(buffer: Buffer): COTSetup {
    if (buffer.length < 1 + 4 + (this.BIT_LENGTH * 65)) {
      return { points_A: Buffer.alloc(0), points_B: Buffer.alloc(0), correlation_x: 0, success: false };
    }
    
    let offset = 0;
    
    const success = buffer[offset++] === 1;
    
    const correlationX = buffer.readUInt32LE(offset);
    offset += 4;
    
    const pointsB = buffer.subarray(offset, offset + (this.BIT_LENGTH * 65));
    
    return {
      points_A: Buffer.alloc(0),
      points_B: pointsB,
      correlation_x: correlationX,
      success
    };
  }

  /**
   * Get Alice's private input (for testing)
   */
  getPrivateInput(): number {
    return this.x;
  }

  /**
   * Get Alice's random U values (for testing)
   */
  getRandomU(): number[] {
    return [...this.random_U];
  }
}

// type definitions matching Bob's C++ structures

export interface COTSetup {
  points_A: Buffer;
  points_B?: Buffer;
  correlation_x: number;
  success: boolean;
}

export interface AliceMessages {
  points_A: Buffer;
  encrypted_m0_messages: Buffer;
  encrypted_m1_messages: Buffer;
  success: boolean;
}

export interface COTResult {
  additive_share_U: number;
  success: boolean;
  alice_messages?: AliceMessages;
}