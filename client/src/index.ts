import path from 'path';
import { AliceMTAClient } from './tcp/mta-client.js';

interface MTAConfig {
  host?: string;
  port?: number;
  aliceShare?: number;
  maxRetries?: number;
  retryDelay?: number;
}

const DEFAULT_CONFIG: Required<MTAConfig> = {
  host: 'localhost',
  port: 8080,
  aliceShare: 0,
  maxRetries: 3,
  retryDelay: 2000
};

function generateRandomShare(): number {
  return Math.floor(Math.random() * 1000000) + 1;
}
function parseArguments(): MTAConfig {
  const args = process.argv.slice(2);
  const config: MTAConfig = {};
  
  if (args.length >= 1 && args[0] !== 'default') {
    config.host = args[0];
  }
  
  if (args.length >= 2) {
    const port = parseInt(args[1]);
    if (!isNaN(port) && port > 0 && port <= 65535) {
      config.port = port;
    } else {
      console.warn(`Invalid port number: ${args[1]}, using default: ${DEFAULT_CONFIG.port}`);
    }
  }
  
  if (args.length >= 3) {
    const share = parseInt(args[2]);
    if (!isNaN(share) && share > 0) {
      config.aliceShare = share;
    } else {
      console.warn(`Invalid Alice share: ${args[2]}, will generate random share`);
    }
  }
  
  return config;
}

function delay(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function executeMTAWithRetry(config: Required<MTAConfig>): Promise<boolean> {
  let lastError: Error | null = null;
  
  for (let attempt = 1; attempt <= config.maxRetries; attempt++) {
    try {
      console.log(`\n=== MTA PROTOCOL ATTEMPT ${attempt}/${config.maxRetries} ===`);
      
      const protoPath = path.resolve(__dirname, '../../proto/mta.proto'); // adjust if needed
      const client = await AliceMTAClient.create(protoPath, config.host, config.port);

      const cleanup = () => {
        console.log('\nReceived shutdown signal, closing connection...');
        client.destroy();
        process.exit(0);
      };
      
      process.on('SIGINT', cleanup);
      process.on('SIGTERM', cleanup);
      
      await client.startMTA(config.aliceShare);
      
      client.close();
      
      return true;
      
    } catch (error) {
      lastError = error as Error;
      console.error(`Attempt ${attempt} failed:`, error.message);
      
      if (attempt < config.maxRetries) {
        const delayMs = config.retryDelay * Math.pow(2, attempt - 1);
        console.log(`Retrying in ${delayMs}ms...`);
        await delay(delayMs);
      }
    }
  }
  
  console.error(`\n=== MTA PROTOCOL FAILED ===`);
  console.error(`All ${config.maxRetries} attempts failed.`);
  console.error(`Last error: ${lastError?.message || 'Unknown error'}`);
  console.error('===========================\n');
  
  return false;
}

/**
 * validate server connectivity
 */
async function validateServerConnection(host: string, port: number): Promise<boolean> {
  return new Promise((resolve) => {
    const net = require('net');
    const socket = new net.Socket();
    
    const timeout = setTimeout(() => {
      socket.destroy();
      resolve(false);
    }, 5000);
    
    socket.on('connect', () => {
      clearTimeout(timeout);
      socket.destroy();
      resolve(true);
    });
    
    socket.on('error', () => {
      clearTimeout(timeout);
      resolve(false);
    });
    
    socket.connect(port, host);
  });
}

async function main(): Promise<void> {
  try {
    const userConfig = parseArguments();
    const config: Required<MTAConfig> = { ...DEFAULT_CONFIG, ...userConfig };
    
    if (config.aliceShare === 0) {
      config.aliceShare = generateRandomShare();
    }
    
    console.log('=== MTA CLIENT (ALICE) CONFIGURATION ===');
    console.log(`Host: ${config.host}`);
    console.log(`Port: ${config.port}`);
    console.log(`Alice's Multiplicative Share: ${config.aliceShare}`);
    console.log(`Max Retries: ${config.maxRetries}`);
    console.log(`Retry Delay: ${config.retryDelay}ms`);
    
    console.log('Checking server connectivity...');
    const isServerReachable = await validateServerConnection(config.host, config.port);
    
    if (!isServerReachable) {
      console.error(`Cannot reach server at ${config.host}:${config.port}`);
      console.error('Please ensure the MTA server (Bob) is running.');
      console.error('Start the server with: ./mta_server [port] [bob_share]');
      process.exit(1);
    }
    
    console.log(`Server at ${config.host}:${config.port} is reachable\n`);
    
    const success = await executeMTAWithRetry(config);
    
    if (success) {
      console.log('MTA Protocol executed successfully.');
      process.exit(0);
    } else {
      console.error('MTA Protocol execution failed after all retries.');
      process.exit(1);
    }
    
  } catch (error) {
    console.error('Fatal error in main execution:', error);
    process.exit(1);
  }
}

process.on('uncaughtException', (error) => {
  console.error('Uncaught Exception:', error);
  process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('Unhandled Rejection at:', promise, 'reason:', reason);
  process.exit(1);
});

if (require.main === module) {
  main().catch((error) => {
    console.error('Error in main:', error);
    process.exit(1);
  });
}