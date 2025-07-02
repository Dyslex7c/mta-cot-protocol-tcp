import * as crypto from 'crypto';
import { AliceCOTProtocol, COTSetup, AliceMessages as COTAliceMessages, COTResult } from './cot-protocol.js';

export interface BobSetup {
  points_B: Buffer;
  correlation_delta: number;
  success: boolean;
}

export interface AliceMessages {
  masked_share: number;
  points_A: Buffer;
  encrypted_m0_messages: Buffer;
  encrypted_m1_messages: Buffer;
  success: boolean;
}

export interface BobMessages {
  masked_share: number;
  success: boolean;
}

export interface MTAResult {
  additive_share: number;
  success: boolean;
}

/**
 * Alice's implementation of the MTA (Multiplication to Addition) Protocol
 * Compatible with Bob's C++ MTA implementation for secure multiplication
 */
export class AliceMTAProtocol {
  private cotProtocol: AliceCOTProtocol;
  private alpha: number = 0;

  constructor() {
    this.cotProtocol = new AliceCOTProtocol();
  }

  /**
   * Generate a cryptographically secure random 32-bit unsigned integer
   */
  private generateRandomUint32(): number {
    const randomBytes = crypto.randomBytes(4);
    return randomBytes.readUInt32LE(0);
  }

  /**
   * Compute final share for verification purposes
   */
  computeFinalShare(receivedShare: number, mask: number, ownShare: number): number {
    return (receivedShare + mask * ownShare) >>> 0; // Ensure 32-bit unsigned
  }

  /**
   * Validate MTA inputs (basic validation)
   */
  private validateMTAInputs(share1: number, share2: number): boolean {
    return Number.isInteger(share1) && Number.isInteger(share2) &&
           share1 >= 0 && share1 <= 0xFFFFFFFF &&
           share2 >= 0 && share2 <= 0xFFFFFFFF;
  }

  /**
   * Initialize Alice's MTA protocol with her multiplicative input
   */
  initializeAsAlice(xShare: number): COTSetup {
    const cotSetup = this.cotProtocol.initializeCOT(xShare);
    
    if (!cotSetup.success) {
      console.error('Failed to initialize COT protocol');
      return {
        points_A: Buffer.alloc(0),
        points_B: Buffer.alloc(0),
        correlation_x: 0,
        success: false
      };
    }

    console.log(`Alice initialized COT with x_share: ${xShare}`);
    return cotSetup;
  }

  /**
 * Prepare Alice's messages after receiving Bob's setup.
 * Uses performCOTMultiplication to ensure correct initialization.
 */
async prepareAliceMessages(xShare: number, bobSetup: BobSetup): Promise<AliceMessages> {
  const result: AliceMessages = {
    masked_share: 0,
    points_A: Buffer.alloc(0),
    encrypted_m0_messages: Buffer.alloc(0),
    encrypted_m1_messages: Buffer.alloc(0),
    success: false
  };

  if (!bobSetup.success) {
    console.error('Bob setup is invalid');
    return result;
  }

  this.alpha = this.generateRandomUint32();
  
  result.masked_share = (xShare * this.alpha) >>> 0;

  const cotSetup: COTSetup = {
    points_A: Buffer.alloc(0),
    points_B: bobSetup.points_B,
    correlation_x: xShare,
    success: true
  };

  try {
    const cotResult: COTResult = await this.cotProtocol.performCOTMultiplication(xShare, cotSetup);

    if (!cotResult.success || !cotResult.alice_messages) {
      console.error('Failed to perform COT multiplication');
      return result;
    }

    result.points_A = cotResult.alice_messages.points_A;
    result.encrypted_m0_messages = cotResult.alice_messages.encrypted_m0_messages;
    result.encrypted_m1_messages = cotResult.alice_messages.encrypted_m1_messages;
    result.success = true;

    console.log(`Alice prepared messages with x_share: ${xShare}, alpha: ${this.alpha}, masked_share: ${result.masked_share}`);
    return result;
  } catch (error) {
    console.error('Error during prepareAliceMessages:', error);
    return result;
  }
}

  /**
   * Execute Alice's side of the MTA protocol
   */
  async executeAliceMTA(xShare: number, bobMessages: BobMessages): Promise<MTAResult> {
    const result: MTAResult = {
      additive_share: 0,
      success: false
    };

    if (!bobMessages.success) {
      console.error('Bob messages are invalid');
      return result;
    }

    console.log(`Executing Alice MTA with x_share: ${xShare}`);

    try {
      const cotAdditiveShare = this.cotProtocol.calculateAdditiveShare();
            
      result.additive_share = cotAdditiveShare;
      result.success = true;

      console.log(`Alice's final additive share: ${result.additive_share}`);

      return result;
    } catch (error) {
      console.error('Alice MTA execution failed:', error);
      return result;
    }
  }

  /**
   * Complete MTA protocol execution (convenience method)
   */
  async performMTA(xShare: number, bobSetup: BobSetup, bobMessages: BobMessages): Promise<{
    aliceMessages: AliceMessages;
    mtaResult: MTAResult;
  }> {
    const aliceSetup = this.initializeAsAlice(xShare);
    if (!aliceSetup.success) {
      return {
        aliceMessages: {
          masked_share: 0,
          points_A: Buffer.alloc(0),
          encrypted_m0_messages: Buffer.alloc(0),
          encrypted_m1_messages: Buffer.alloc(0),
          success: false
        },
        mtaResult: { additive_share: 0, success: false }
      };
    }

    const aliceMessages = await this.prepareAliceMessages(xShare, bobSetup);
    if (!aliceMessages.success) {
      return {
        aliceMessages,
        mtaResult: { additive_share: 0, success: false }
      };
    }

    const mtaResult = await this.executeAliceMTA(xShare, bobMessages);
    
    return {
      aliceMessages,
      mtaResult
    };
  }

  serializeBobSetup(setup: BobSetup): Buffer {
    const buffer = Buffer.alloc(1 + 4 + setup.points_B.length);
    let offset = 0;

    buffer[offset++] = setup.success ? 1 : 0;

    // correlation_delta (4 bytes, little-endian)
    buffer.writeUInt32LE(setup.correlation_delta, offset);
    offset += 4;

    setup.points_B.copy(buffer, offset);

    return buffer;
  }

  deserializeBobSetup(buffer: Buffer): BobSetup {
    if (buffer.length < 5) {
      return {
        points_B: Buffer.alloc(0),
        correlation_delta: 0,
        success: false
      };
    }

    let offset = 0;

    const success = buffer[offset++] === 1;

    const correlationDelta = buffer.readUInt32LE(offset);
    offset += 4;

    const pointsB = buffer.subarray(offset);

    return {
      points_B: pointsB,
      correlation_delta: correlationDelta,
      success
    };
  }

  serializeAliceMessages(messages: AliceMessages): Buffer {
    const totalSize = 1 + 4 + messages.points_A.length + 
                     messages.encrypted_m0_messages.length + 
                     messages.encrypted_m1_messages.length;
    
    const buffer = Buffer.alloc(totalSize);
    let offset = 0;

    buffer[offset++] = messages.success ? 1 : 0;

    buffer.writeUInt32LE(messages.masked_share, offset);
    offset += 4;

    messages.points_A.copy(buffer, offset);
    offset += messages.points_A.length;

    messages.encrypted_m0_messages.copy(buffer, offset);
    offset += messages.encrypted_m0_messages.length;

    messages.encrypted_m1_messages.copy(buffer, offset);

    return buffer;
  }

  deserializeAliceMessages(buffer: Buffer): AliceMessages {
    if (buffer.length < 5) {
      return {
        masked_share: 0,
        points_A: Buffer.alloc(0),
        encrypted_m0_messages: Buffer.alloc(0),
        encrypted_m1_messages: Buffer.alloc(0),
        success: false
      };
    }

    let offset = 0;

    const success = buffer[offset++] === 1;

    const maskedShare = buffer.readUInt32LE(offset);
    offset += 4;

    const pointsSize = 32 * 65;
    const messagesSize = 32 * 32;

    if (buffer.length < offset + pointsSize + 2 * messagesSize) {
      return {
        masked_share: maskedShare,
        points_A: Buffer.alloc(0),
        encrypted_m0_messages: Buffer.alloc(0),
        encrypted_m1_messages: Buffer.alloc(0),
        success: false
      };
    }

    const pointsA = buffer.subarray(offset, offset + pointsSize);
    offset += pointsSize;

    const encryptedM0Messages = buffer.subarray(offset, offset + messagesSize);
    offset += messagesSize;

    const encryptedM1Messages = buffer.subarray(offset, offset + messagesSize);

    return {
      masked_share: maskedShare,
      points_A: pointsA,
      encrypted_m0_messages: encryptedM0Messages,
      encrypted_m1_messages: encryptedM1Messages,
      success
    };
  }

  serializeBobMessages(messages: BobMessages): Buffer {
    const buffer = Buffer.alloc(5);
    let offset = 0;

    buffer[offset++] = messages.success ? 1 : 0;

    buffer.writeUInt32LE(messages.masked_share, offset);

    return buffer;
  }

  deserializeBobMessages(buffer: Buffer): BobMessages {
    if (buffer.length !== 5) {
      return {
        masked_share: 0,
        success: false
      };
    }

    let offset = 0;

    const success = buffer[offset++] === 1;

    const maskedShare = buffer.readUInt32LE(offset);

    return {
      masked_share: maskedShare,
      success
    };
  }

  serializeMTAResult(result: MTAResult): Buffer {
    const buffer = Buffer.alloc(5);
    let offset = 0;

    buffer[offset++] = result.success ? 1 : 0;

    buffer.writeUInt32LE(result.additive_share, offset);

    return buffer;
  }

  deserializeMTAResult(buffer: Buffer): MTAResult {
    if (buffer.length !== 5) {
      return {
        additive_share: 0,
        success: false
      };
    }

    let offset = 0;

    const success = buffer[offset++] === 1;

    const additiveShare = buffer.readUInt32LE(offset);

    return {
      additive_share: additiveShare,
      success
    };
  }

  getAlpha(): number {
    return this.alpha;
  }
  getCOTProtocol(): AliceCOTProtocol {
    return this.cotProtocol;
  }
}