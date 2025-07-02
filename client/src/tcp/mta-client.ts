import * as net from 'net';
import { 
  MTAProtobufHandler, 
  ClientState, 
  IAliceMessages, 
  IBobSetup, 
  IBobMessages,
  BackwardCompatibilityAdapter
} from '../protobuf/protobuf-handler.js'

import { AliceMTAProtocol, BobSetup, AliceMessages, BobMessages, MTAResult } from '../protocol/mta-protocol.js';

export class AliceMTAClient {
  private socket: net.Socket;
  private mtaProtocol: AliceMTAProtocol;
  private protobufHandler: MTAProtobufHandler | undefined;
  private state: ClientState;
  private readBuffer: Buffer;
  private expectedMessageSize: number;
  private aliceXShare: number;
  private aliceAdditiveShare: number;

  constructor(private host: string = 'localhost', private port: number = 8080) {
    this.socket = new net.Socket();
    this.mtaProtocol = new AliceMTAProtocol();
    this.state = ClientState.CONNECTING;
    this.readBuffer = Buffer.alloc(0);
    this.expectedMessageSize = 0;
    this.aliceXShare = 0;
    this.aliceAdditiveShare = 0;
    
    this.setupSocketHandlers();
  }

  static async create(protoPath: string, host?: string, port?: number): Promise<AliceMTAClient> {
    const client = new AliceMTAClient(host, port);
    client.protobufHandler = await MTAProtobufHandler.create(protoPath);
    return client;
  }

  private setupSocketHandlers(): void {
    this.socket.on('connect', () => {
      console.log(`Connected to MTA server at ${this.host}:${this.port}`);
      this.state = ClientState.SENDING_CORRELATION_DELTA;
    });

    this.socket.on('data', (data: Buffer) => {
      this.readBuffer = Buffer.concat([this.readBuffer, data]);
      this.processReceivedData();
    });

    this.socket.on('close', () => {
      console.log('Connection to MTA server closed');
    });

    this.socket.on('error', (error: Error) => {
      console.error('Socket error:', error.message);
    });
  }

  private processReceivedData(): void {
    while (true) {
      if (this.expectedMessageSize === 0) {
        if (this.readBuffer.length < 4) break;
  
        this.expectedMessageSize = this.readBuffer.readUInt32LE(0);
        this.readBuffer = this.readBuffer.subarray(4);
  
        console.log(`[RECV] Incoming message size header: ${this.expectedMessageSize} bytes`);
      }
  
      if (this.readBuffer.length < this.expectedMessageSize) {
        console.log(`[RECV] Incomplete message: need ${this.expectedMessageSize}, have ${this.readBuffer.length}`);
        break;
      }
  
      const messageData = this.readBuffer.subarray(0, this.expectedMessageSize);
      this.readBuffer = this.readBuffer.subarray(this.expectedMessageSize);
      this.expectedMessageSize = 0;
  
      console.log(`[RECV] Full message received (${messageData.length} bytes)`);
      this.processMessage(messageData);
    }
  }
  

  private processMessage(data: Buffer): void {
    const messageBytes = new Uint8Array(data);
    
    switch (this.state) {
      case ClientState.WAITING_FOR_BOB_SETUP:
        this.processBobSetup(messageBytes);
        break;
        
      case ClientState.WAITING_FOR_BOB_MESSAGES:
        this.processBobMessages(messageBytes);
        break;
        
      default:
        console.error(`Unexpected message received in state: ${this.state}`);
        break;
    }
  }

  private async processBobSetup(data: Uint8Array): Promise<void> {
    console.log("[DEBUG] Attempting to deserialize BobSetup");
    console.log("[DEBUG] First 20 bytes of message:", Buffer.from(data.slice(0, 20)).toString('hex'));
    console.log("[DEBUG] Total buffer length:", data.length);
  
    try {
      const decoded = this.protobufHandler!.deserializeBobSetup(data);
      if (!decoded) {
        console.error('[ERROR] Deserialization returned null (BobSetup)');
        return;
      }
  
      const bobSetup = decoded;
      console.log('[SUCCESS] BobSetup deserialized');
  
      if (!bobSetup.success) {
        console.error('Bob setup failed');
        return;
      }
  
      const allPoints = Buffer.concat(bobSetup.otMessages);

      const internalBobSetup: BobSetup = {
        points_B: allPoints,
        correlation_delta: 0,
        success: bobSetup.success
      };
  
      const aliceMessages = await this.mtaProtocol.prepareAliceMessages(this.aliceXShare, internalBobSetup);
      
      if (!aliceMessages.success) {
        console.error('Failed to prepare Alice messages');
        return;
      }
  
      const protoAliceMessages: IAliceMessages = {
        maskedShare: aliceMessages.masked_share,
        otChoices: this.generateOTChoices(bobSetup.numOtInstances),
        encryptedShares: this.generateEncryptedShares(bobSetup.numOtInstances)
      };
  
      this.state = ClientState.SENDING_ALICE_MESSAGES;
      this.sendAliceMessages(protoAliceMessages);
    } catch (err) {
      console.error('[ERROR] Failed to deserialize BobSetup:', err);
    }
  }    

  private processBobMessages(data: Uint8Array): void {
    const bobMessages = this.protobufHandler.deserializeBobMessages(data);
    if (!bobMessages) {
      console.error('Failed to deserialize Bob messages');
      return;
    }
    
    console.log('Received Bob messages successfully');
    
    if (!bobMessages.success) {
      console.error('Bob messages indicate failure');
      return;
    }    
    const internalBobMessages: BobMessages = {
      masked_share: bobMessages.masked_share,
      success: bobMessages.success
    };
    
    this.mtaProtocol.executeAliceMTA(this.aliceXShare, internalBobMessages)
      .then((mtaResult: MTAResult) => {
        if (mtaResult.success) {
          this.aliceAdditiveShare = mtaResult.additive_share;
          
          console.log('\n=== MTA PROTOCOL COMPUTATION COMPLETED ===');
          console.log(`Alice's Multiplicative Share: ${this.aliceXShare}`);
          console.log(`Alice's Additive Share: ${this.aliceAdditiveShare}`);
          
          this.state = ClientState.PROTOCOL_COMPLETE;
        } else {
          console.error('Alice MTA execution failed');
        }
      })
      .catch((error) => {
        console.error('Error executing Alice MTA:', error);
      });
  }

  private generateOTChoices(numInstances: number): boolean[] {
    const choices: boolean[] = [];
    const xBits = this.aliceXShare.toString(2).padStart(32, '0').split('').reverse();
  
    for (let i = 0; i < numInstances; i++) {
      const bit = xBits[i % 32];
      choices.push(bit === '1');
    }
  
    return choices;
  }  

  private generateEncryptedShares(numInstances: number): Uint8Array[] {
    const shares: Uint8Array[] = [];
    for (let i = 0; i < numInstances; i++) {
      const share = new Uint8Array(32);
      for (let j = 0; j < 32; j++) {
        share[j] = (this.aliceXShare + i + j) & 0xFF;
      }
      shares.push(share);
    }
    return shares;
  }

  private sendMessageWithSize(message: Uint8Array): void {
    const sizeBuffer = Buffer.allocUnsafe(4);
    sizeBuffer.writeUInt32LE(message.length, 0);
    const completeMessage = Buffer.concat([sizeBuffer, Buffer.from(message)]);
  
    console.log(`[SEND] Sending message of ${message.length} bytes (+ 4-byte header)`);
  
    this.socket.write(completeMessage, (error) => {
      if (error) {
        console.error('[ERROR] Message send failed:', error.message);
      } else {
        console.log(`[SEND] Message sent (${completeMessage.length} total bytes)`);
      }
    });
  }    

  private sendCorrelationDelta(): void {
    try {
      const correlationDelta = Math.floor(Math.random() * 1_000_000) + 1;
      console.log(`Sending correlation delta: ${correlationDelta}`);
  
      const serializedDelta = this.protobufHandler!.serializeCorrelationDelta(correlationDelta);
      const buffer = Buffer.from(serializedDelta);
  
      console.log("Serialized correlation data:", buffer);
      this.sendMessageWithSize(serializedDelta);
      this.state = ClientState.WAITING_FOR_BOB_SETUP;
  
      console.log('Waiting for Bob setup...');
    } catch (error) {
      console.error('Failed to send correlation delta:', error);
    }
  }
  

  private sendAliceMessages(messages: IAliceMessages): void {
    try {
      console.log('Sending Alice messages...');
      console.log(`Alice's masked share: ${messages.maskedShare}`);
      console.log(`OT choices count: ${messages.otChoices.length}`);
      console.log(`Encrypted shares count: ${messages.encryptedShares.length}`);
      
      const serializedMessages = this.protobufHandler.serializeAliceMessages(messages);
      this.sendMessageWithSize(serializedMessages);
      
      this.state = ClientState.WAITING_FOR_BOB_MESSAGES;
      console.log('Waiting for Bob messages...');
    } catch (error) {
      console.error('Failed to send Alice messages:', error);
    }
  }

  /**
   * Start the MTA protocol with Alice's multiplicative share
   */
  public async startMTA(xShare: number): Promise<void> {
    return new Promise((resolve, reject) => {
      this.aliceXShare = xShare;
      
      console.log(`Alice's multiplicative share (x): ${xShare}`);
      console.log(`Connecting to ${this.host}:${this.port}...`);
      
      this.socket.on('connect', () => {
        setTimeout(() => {
          this.sendCorrelationDelta();
        }, 100);
      });
      
      this.socket.on('close', () => {
        if (this.state === ClientState.PROTOCOL_COMPLETE) {
          resolve();
        } else {
          reject(new Error('Connection closed before protocol completion'));
        }
      });
      
      this.socket.on('error', (error) => {
        reject(error);
      });
      
      this.socket.connect(this.port, this.host);
    });
  }

  /**
   * Get Alice's computed additive share
   */
  public getAdditiveShare(): number {
    return this.aliceAdditiveShare;
  }

  /**
   * Get the current protocol state
   */
  public getState(): ClientState {
    return this.state;
  }

  /**
   * Get protocol statistics
   */
  public getProtocolStats(): { state: string; xShare: number; additiveShare: number } {
    return {
      state: ClientState[this.state],
      xShare: this.aliceXShare,
      additiveShare: this.aliceAdditiveShare
    };
  }

  /**
   * Validate a message before sending (for debugging)
   */
  public validateMessage(messageType: string, payload: any): string | null {
    return this.protobufHandler.validateMessage(messageType, payload);
  }

  /**
   * Close the connection gracefully
   */
  public close(): void {
    console.log('Closing MTA client connection');
    this.socket.end();
  }

  /**
   * Force close the connection
   */
  public destroy(): void {
    console.log('Destroying MTA client connection');
    this.socket.destroy();
  }
}