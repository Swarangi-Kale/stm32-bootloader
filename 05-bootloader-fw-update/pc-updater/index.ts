import * as fs from 'fs/promises';
import * as path from 'path';
import {SerialPort} from 'serialport';
import { Buffer } from 'node:buffer';

const FLASH_BASE_ADDR = (0x08000000)
const BOOTLOADER_SIZE = (0x8000)
const APP_START_ADDR = (FLASH_BASE_ADDR + BOOTLOADER_SIZE)


// Constants for the packet protocol
const PACKET_LENGTH_BYTES   = 1;
const PACKET_DATA_BYTES     = 16;
const PACKET_CRC_BYTES      = 1;
const PACKET_CRC_INDEX      = PACKET_LENGTH_BYTES + PACKET_DATA_BYTES;
const PACKET_LENGTH         = PACKET_LENGTH_BYTES + PACKET_DATA_BYTES + PACKET_CRC_BYTES;
const APP_SIZE = 1024 * 96; //as defined in the linker script
const CHUNK_SIZE = PACKET_DATA_BYTES; // 16

const PACKET_ACK_DATA0      = 0x15;
const PACKET_RETX_DATA0     = 0x19;

// Details about the serial port connection
const serialPath            = "COM5";
const baudRate              = 115200;

const BL_PACKET_SEQ_OBSERVED_DATA0 = 0X20;
const BL_PACKET_FW_UPDATE_REQ_DATA0 = 0X28;
const BL_PACKET_FW_UPDATE_RES_DATA0 = 0X37; //Fw update request accepted
const BL_PACKET_DEVICE_ID_REQ_DATA0 = 0X3A;
const BL_PACKET_DEVICE_ID_RES_DATA0 = 0X3E;
const BL_PACKET_FW_LENGTH_REQ_DATA0 = 0X42;
const BL_PACKET_FW_LENGTH_RES_DATA0 = 0X49;
const BL_PACKET_READY_FOR_DATA_DATA0 = 0X4F;
const BL_PACKET_UPDATE_SUCCESSFUL_DATA0 = 0X51;

const BL_PACKET_FLASH_NOT_WRITTEN_DATA0 = 0x77;

const SYNC_SEQ = Buffer.from([0x13,0x43,0x2A,0x78]);
const DEFAULT_TIMEOUT = 5000;

let DEVICE_ID = 0;

const HEADER_OFFSETS = {
  sentinel:   0,
  device_id:  4,
  version:    8,
  fw_length:  12,
  reserved0:  16,
  reserved1:  20,
  reserved2:  24,
  reserved3:  28,
  crc32:      32,
}

let fwImage : Buffer;
let fwLength = 0;

// CRC8 implementation
const crc8 = (data: Buffer | Array<number>) => {
  let crc = 0;

  for (const byte of data) {
    crc = (crc ^ byte) & 0xff;
    for (let i = 0; i < 8; i++) {
      if (crc & 0x80) {
        crc = ((crc << 1) ^ 0x07) & 0xff;
      } else {
        crc = (crc << 1) & 0xff;
      }
    }
    // console.log(`0x${byte.toString(16)} 0x${crc.toString(16)}`);
  }

  return crc;
};

const crc32 = (data: Buffer, length: number) => {
  let byte;
  let crc = 0xffffffff;
  let mask;

  for (let i = 0; i < length; i++) {
     byte = data[i];
     crc = (crc ^ byte) >>> 0;

     for (let j = 0; j < 8; j++) {
        mask = (-(crc & 1)) >>> 0;
        crc = ((crc >>> 1) ^ (0xedb88320 & mask)) >>> 0;
     }
  }

  return (~crc) >>> 0;
}

// Async delay function, which gives the event loop time to process outside input
const delay = (ms: number) => new Promise(r => setTimeout(r, ms));

class Logger {
  static info(message: string) { console.log(`[.] ${message}`); }
  static success(message: string) { console.log(`[$] ${message}`); }
  static error(message: string) { console.log(`[!] ${message}`); }
}

// Class for serialising and deserialising packets
class Packet {
  length: number;
  data: Buffer;
  crc: number;

  static retx = new Packet(1, Buffer.from([PACKET_RETX_DATA0])).toBuffer();
  static ack = new Packet(1, Buffer.from([PACKET_ACK_DATA0])).toBuffer();

  constructor(length: number, data: Buffer, crc?: number) {
    this.length = length;
    this.data = data;

    const bytesToPad = PACKET_DATA_BYTES - this.data.length;
    const padding = Buffer.alloc(bytesToPad).fill(0xff);
    this.data = Buffer.concat([this.data, padding]);

    if (typeof crc === 'undefined') {
      this.crc = this.computeCrc();
    } else {
      this.crc = crc;
    }
  }

  computeCrc() {
    const allData = [this.length, ...this.data];
    return crc8(allData);
  }

  toBuffer() {
    return Buffer.concat([ Buffer.from([this.length]), this.data, Buffer.from([this.crc]) ]);
  }

  isSingleBytePacket(byte: number) {
    if (this.length !== 1) return false;
    if (this.data[0] !== byte) return false;
    for (let i = 1; i < PACKET_DATA_BYTES; i++) {
      if (this.data[i] !== 0xff) return false;
    }
    return true;
  }

  waitForSingleBytePacket(byte: number) {
    if (this.length !== 1) return false;
    if (this.data[0] !== byte) return false;
    for (let i = 1; i < PACKET_DATA_BYTES; i++) {
      if (this.data[i] !== 0xff) return false;
    }
    return true;
  }

  isAck() {
    return this.isSingleBytePacket(PACKET_ACK_DATA0);
  }

  isRetx() {
    return this.isSingleBytePacket(PACKET_RETX_DATA0);
  }
}

// Serial port instance
const uart = new SerialPort({ path: serialPath, baudRate });

uart.on('open', () => {
  console.log('Serial port opened successfully');
});

uart.on('error', (err) => {
  console.error('Serial port error:', err.message);
});

// Packet buffer
let packets: Packet[] = [];

let lastPacket: Buffer = Packet.ack;

let ackReceived = false;

// const writePacket = (packet: Buffer) => {
//   console.log(`inside writePacket function`);
//   uart.write(packet);
//   lastPacket = packet;
// };

const writePacket = (packet: Buffer) => {
  // console.log(`inside writePacket function`);
  uart.write(packet);
  lastPacket = packet;
};

// Serial data buffer, with a splice-like function for consuming data
let rxBuffer = Buffer.from([]);
const consumeFromBuffer = (n: number) => {
  const consumed = rxBuffer.slice(0, n);
  rxBuffer = rxBuffer.slice(n);
  return consumed;
}

// This function fires whenever data is received over the serial port. The whole
// packet state machine runs here.
uart.on('data', data => {
  // console.log(`Received ${data.length} bytes through uart`);
  // console.log(data);
  // Add the data to the packet
  rxBuffer = Buffer.concat([rxBuffer, data]);

  // Can we build a packet?
  while (rxBuffer.length >= PACKET_LENGTH) {
    // console.log(`Building a packet`);
    const raw = consumeFromBuffer(PACKET_LENGTH);
    // console.log(raw);
    const packet = new Packet(raw[0], raw.slice(1, 1+PACKET_DATA_BYTES), raw[PACKET_CRC_INDEX]);
    // console.log(packet);
    const computedCrc = packet.computeCrc();

    // Need retransmission?
    if (packet.crc !== computedCrc) {
      // console.log(`CRC failed, computed 0x${computedCrc.toString(16)}, got 0x${packet.crc.toString(16)}`);
      // console.log(`Asking for retransmission`);
      writePacket(Packet.retx);
      continue;
    }

    // Are we being asked to retransmit?
    if (packet.isRetx()) {
      // console.log(`Retransmitting last packet`);
      // console.log(`Last packet:`, lastPacket);
      writePacket(lastPacket);
      continue;
    }

    // If this is an ack, move on
    if (packet.isAck()) {
      // console.log(`It was an Ack, moving on`);
      // console.log(packet);
      ackReceived = true;
      continue;
    }

    // Otherwise write the packet in to the buffer, and send an ack
    // console.log(`Storing packet`);
    packets.push(packet);
    writePacket(Packet.ack);
    // console.log(`Ack sent`);
  }
});

// Function to allow us to await a packet
const waitForPacket = async () => {
  while (packets.length < 1) {
    await delay(1);
  }
  return packets.splice(0,1)[0];
}


// Helper to wait for ackReceived flag to flip true
const waitForAck = async (timeoutMs = 20000) => {
  const start = Date.now();
  while (!ackReceived) {
    if (Date.now() - start > timeoutMs) {
      throw new Error('Timed out waiting for ACK');
    }
    await delay(1);
  }
  ackReceived = false; // reset for next use
}

// console.log(Packet.ack);
const readingNewFirmwareImage = async () => {
  fwImage = await fs.readFile(path.join(process.cwd(), 'application.bin'));
  fwLength = fwImage.length;
};


const syncWithBootloader = async (timeout = DEFAULT_TIMEOUT) => {
  let timeWaited = 0;

  while(true) {
    uart.write(SYNC_SEQ);
    await delay(1000);

    if (packets.length > 0) {
      const packet = packets.splice(0,1)[0];
      if (packet.isSingleBytePacket(BL_PACKET_SEQ_OBSERVED_DATA0)) {
        return;
      }
      Logger.error('Wrong Packet Observed In Sync Sequence');
      process.exit(1);
    }

    if (timeWaited >= timeout){
      Logger.error('Timed out waiting for sync sequence')
      process.exit(1);
    }

  }
}

const createSingleBytePacket = (byte: number) => {
  return new Packet(1, Buffer.from([byte]));
};

const requestFwUpdate = async () => {
  Logger.info('Requesting firmware update');

  const reqPacket = createSingleBytePacket(BL_PACKET_FW_UPDATE_REQ_DATA0);
  writePacket(reqPacket.toBuffer());

  // Transport-level ack for our request packet
  await waitForAck();

  // Application-level response from the bootloader
  const response = await waitForPacket();
  if (!response.isSingleBytePacket(BL_PACKET_FW_UPDATE_RES_DATA0)) {
    Logger.error('Bootloader rejected or did not confirm FW update request');
    process.exit(1);
  }

  Logger.success('FW update request accepted');
};

const respondToDeviceIdRequest = async () => {
  Logger.info('Waiting for device ID request');

  const request = await waitForPacket();
  if (!request.isSingleBytePacket(BL_PACKET_DEVICE_ID_REQ_DATA0)) {
    Logger.error('Expected device ID request, got something else');
    process.exit(1);
  }

  const resPacket = new Packet(2, Buffer.from([BL_PACKET_DEVICE_ID_RES_DATA0, DEVICE_ID]));
  writePacket(resPacket.toBuffer());

  // Transport-level ack for our response packet
  await waitForAck();

  Logger.success('Device ID sent');
};

const respondToFirmwareLengthReq = async () => {
  Logger.info('Waiting for firmware length request');

  const request = await waitForPacket();
  if (!request.isSingleBytePacket(BL_PACKET_FW_LENGTH_REQ_DATA0)) {
    Logger.error('Expected firmware length request, got something else');
    process.exit(1);
  }

  const lengthBytes = Buffer.alloc(4);
  lengthBytes.writeUInt32LE(fwLength, 0);

  const resPacket = new Packet(5, Buffer.concat([Buffer.from([BL_PACKET_FW_LENGTH_RES_DATA0]), lengthBytes]));
  writePacket(resPacket.toBuffer());
  console.log(resPacket);

  // Transport-level ack for our response packet
  await waitForAck();

  Logger.success('Firmware Length sent');
}

const waitForReadyForData = async () => {
  Logger.info('Waiting for bootloader to erase app and signal ready');

  const packet = await waitForPacket();
  console.log(`ready for packet being received:`);
  console.log(packet);
  if (!packet.isSingleBytePacket(BL_PACKET_READY_FOR_DATA_DATA0)) {
    console.log(packet);
    Logger.error('Expected ready-for-data packet, got something else');
    process.exit(1);
  }

  Logger.success('Bootloader ready to receive firmware');
};

const sendFirmware = async () => {
  Logger.info('Sending firmware image');

  let offset = 0;
  while (offset < fwImage.length) {
    const data = fwImage.subarray(offset, offset + CHUNK_SIZE);
    const dataLength = data.length;
    const packet = new Packet(dataLength - 1, data);
    writePacket(packet.toBuffer());
    offset+=dataLength;

    // await waitForAck();
    // console.log('ack received');

    const response = await waitForPacket();
    console.log('response packet received');

    if (response.isSingleBytePacket(BL_PACKET_FLASH_NOT_WRITTEN_DATA0)) {
      console.log(response);
      Logger.error(`Flash didn't work properly`);
      process.exit(1);
    }

    if (!response.isSingleBytePacket(BL_PACKET_READY_FOR_DATA_DATA0)) {
      console.log(response);
      Logger.error(`Bootloader did not confirm chunk at offset ${offset}`);
      process.exit(1);
    }

    Logger.info(`Wrote ${dataLength} bytes (${offset}/${fwLength})`);
    // console.log(offset);
    // await delay(2);
  }

  Logger.info('Completed the upload');
  const done = await waitForPacket();
  if (!done.isSingleBytePacket(BL_PACKET_UPDATE_SUCCESSFUL_DATA0)) {
    Logger.error('Did not receive update-successful confirmation');
    process.exit(1);
  }

  Logger.success('Firmware update complete');
};


// Do everything in an async function so we can have loops, awaits etc
const main = async () => {
  Logger.info('Reading the firmware image...');
  await readingNewFirmwareImage();
  Logger.success(`Read firmware image. Length is ${fwLength} bytes`);

  //Ensure the firmware fits within the application flash region
  if (fwLength > APP_SIZE) {
    Logger.error(`Firmware image (${fwLength} bytes) exceeds app region size (${APP_SIZE} bytes)`);
    process.exit(1);
  }

  Logger.info('Sending sync packets and waiting for response');
  await syncWithBootloader();
  Logger.success('Synced');

  await requestFwUpdate();
  await respondToDeviceIdRequest();
  await respondToFirmwareLengthReq();
  await waitForReadyForData();
  await sendFirmware();
}

main()
  // .finally(() => uart.close());