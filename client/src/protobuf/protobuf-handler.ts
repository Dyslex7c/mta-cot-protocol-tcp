import * as path from 'path';
import * as protobuf from 'protobufjs';

export interface ICorrelationDelta {
  delta: number;
}

export interface IBobSetup {
  success: boolean;
  otMessages: Uint8Array[];
  publicKey: Uint8Array;
  numOtInstances: number;
}

export interface IAliceMessages {
  maskedShare: number;
  otChoices: boolean[];
  encryptedShares: Uint8Array[];
}

export interface IBobMessages {
  success: boolean;
  otResponses: Uint8Array[];
  encryptedResult: Uint8Array;
  correlationCheck: number;
}

export interface IMTAResult {
  success: boolean;
  additiveShare: number;
  errorMessage?: string;
}

export class MTAProtobufHandler {
  private root: protobuf.Root;
  private correlationDeltaType: protobuf.Type;
  private bobSetupType: protobuf.Type;
  private aliceMessagesType: protobuf.Type;
  private bobMessagesType: protobuf.Type;
  private mtaResultType: protobuf.Type;

  constructor() {}

  static async create(protoPath: string): Promise<MTAProtobufHandler> {
    const handler = new MTAProtobufHandler();
    await handler.initializeProtobuf(protoPath);
    return handler;
  }

  private async initializeProtobuf(protoPath: string): Promise<void> {
    try {
      this.root = await protobuf.load(protoPath)
      
      this.correlationDeltaType = this.root.lookupType('mta.CorrelationDelta');
      this.bobSetupType = this.root.lookupType('mta.BobSetup');
      this.aliceMessagesType = this.root.lookupType('mta.AliceMessages');
      this.bobMessagesType = this.root.lookupType('mta.BobMessages');
      this.mtaResultType = this.root.lookupType('mta.MTAResult');
      
      console.log('Protobuf schema initialized successfully');
    } catch (error) {
      console.error('Failed to initialize protobuf schema:', error);
      throw error;
    }
  }

  serializeCorrelationDelta(delta: number): Uint8Array {
    try {
      const payload: ICorrelationDelta = { delta };
      
      const errMsg = this.correlationDeltaType.verify(payload);
      if (errMsg) {
        throw new Error(`CorrelationDelta verification failed: ${errMsg}`);
      }
      
      const message = this.correlationDeltaType.create(payload);
      return this.correlationDeltaType.encode(message).finish();
    } catch (error) {
      console.error('Failed to serialize CorrelationDelta:', error);
      throw error;
    }
  }

  deserializeBobSetup(data: Uint8Array): IBobSetup | null {
    try {
      console.log("[PROTOBUF DEBUG] Attempting BobSetup decode");
  
      const message = this.bobSetupType.decode(data);      
      const object = this.bobSetupType.toObject(message, {
        bytes: Uint8Array,
        defaults: true,
      });
    
      return {
        success: object.success ?? false,
        otMessages: Array.isArray(object.otMessages)
          ? object.otMessages.map((msg: any) => new Uint8Array(msg))
          : [],
        publicKey: object.publicKey ? new Uint8Array(object.publicKey) : new Uint8Array(),
        numOtInstances: typeof object.numOtInstances === "number" ? object.numOtInstances : 0
      };
    } catch (error) {
      console.error("[PROTOBUF ERROR] BobSetup decode failed:", error);
      console.error("[PROTOBUF ERROR] Buffer (first 20 bytes):", Buffer.from(data.slice(0, 20)).toString("hex"));
      console.error("[PROTOBUF ERROR] Buffer length:", data.length);
      return null;
    }
  }
  
  
  
  /**
   * Serialize AliceMessages
   */
  serializeAliceMessages(messages: IAliceMessages): Uint8Array {
    try {
      const payload = {
        maskedShare: messages.maskedShare,
        otChoices: messages.otChoices,
        encryptedShares: messages.encryptedShares
      };
      
      const errMsg = this.aliceMessagesType.verify(payload);
      if (errMsg) {
        throw new Error(`AliceMessages verification failed: ${errMsg}`);
      }
      
      const message = this.aliceMessagesType.create(payload);
      return this.aliceMessagesType.encode(message).finish();
    } catch (error) {
      console.error('Failed to serialize AliceMessages:', error);
      throw error;
    }
  }

  /**
   * Deserialize BobMessages
   */
  deserializeBobMessages(data: Uint8Array): IBobMessages | null {
    try {
      const message = this.bobMessagesType.decode(data);
      const object = this.bobMessagesType.toObject(message, {
        bytes: Uint8Array,
        defaults: true
      });
      
      return {
        success: object.success || false,
        otResponses: (object.ot_responses || []).map((resp: any) => new Uint8Array(resp)),
        encryptedResult: new Uint8Array(object.encrypted_result || []),
        correlationCheck: object.correlation_check || 0
      };
      
    } catch (error) {
      console.error('Failed to deserialize BobMessages:', error);
      return null;
    }
  }

  /**
   * Serialize MTAResult (for internal use)
   */
  serializeMTAResult(result: IMTAResult): Uint8Array {
    try {
      const payload = {
        success: result.success,
        additiveShare: result.additiveShare,
        errorMessage: result.errorMessage || ''
      };
      
      const errMsg = this.mtaResultType.verify(payload);
      if (errMsg) {
        throw new Error(`MTAResult verification failed: ${errMsg}`);
      }
      
      const message = this.mtaResultType.create(payload);
      return this.mtaResultType.encode(message).finish();
    } catch (error) {
      console.error('Failed to serialize MTAResult:', error);
      throw error;
    }
  }

  /**
   * Deserialize MTAResult (for internal use)
   */
  deserializeMTAResult(data: Uint8Array): IMTAResult | null {
    try {
      const message = this.mtaResultType.decode(data);
      const object = this.mtaResultType.toObject(message, {
        bytes: Uint8Array,
        defaults: true
      });
      
      return {
        success: object.success || false,
        additiveShare: object.additiveShare || 0,
        errorMessage: object.errorMessage || undefined
      };
    } catch (error) {
      console.error('Failed to deserialize MTAResult:', error);
      return null;
    }
  }

  /**
   * Utility method to get the schema root for advanced usage
   */
  getRoot(): protobuf.Root {
    return this.root;
  }

  /**
   * Utility method to validate any message type
   */
  validateMessage(messageType: string, payload: any): string | null {
    try {
      const type = this.root.lookupType(`mta.${messageType}`);
      return type.verify(payload);
    } catch (error) {
      return `Failed to lookup message type: ${error}`;
    }
  }
}

export enum ClientState {
  CONNECTING = 0,
  SENDING_CORRELATION_DELTA = 1,
  WAITING_FOR_BOB_SETUP = 2,
  SENDING_ALICE_MESSAGES = 3,
  WAITING_FOR_BOB_MESSAGES = 4,
  PROTOCOL_COMPLETE = 5
}

export class BackwardCompatibilityAdapter {
  static toBobSetupLegacy(setup: IBobSetup): ProtoBobSetup {
    return {
      ...setup,
      ot_messages: setup.otMessages,
      public_key: setup.publicKey,
      num_ot_instances: setup.numOtInstances
    };
  }

  static fromAliceMessagesLegacy(legacy: ProtoAliceMessages): IAliceMessages {
    return {
      maskedShare: legacy.masked_share,
      otChoices: legacy.ot_choices,
      encryptedShares: legacy.encrypted_shares
    };
  }

  static toBobMessagesLegacy(messages: IBobMessages): ProtoBobMessages {
    return {
      ...messages,
      ot_responses: messages.otResponses,
      encrypted_result: messages.encryptedResult,
      correlation_check: messages.correlationCheck
    };
  }
}

export interface ProtoBobSetup {
  success: boolean;
  ot_messages: Uint8Array[];
  public_key: Uint8Array;
  num_ot_instances: number;
}

export interface ProtoAliceMessages {
  masked_share: number;
  ot_choices: boolean[];
  encrypted_shares: Uint8Array[];
}

export interface ProtoBobMessages {
  success: boolean;
  ot_responses: Uint8Array[];
  encrypted_result: Uint8Array;
  correlation_check: number;
}