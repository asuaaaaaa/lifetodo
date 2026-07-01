#!/usr/bin/env node

const fs = require("fs");

const [input, output, requestedIndex = "0"] = process.argv.slice(2);

if (!input || !output) {
  console.error("Usage: extract-ttc-font.js <input.ttc> <output.ttf> [font-index]");
  process.exit(2);
}

const index = Number.parseInt(requestedIndex, 10);
const source = fs.readFileSync(input);

function u16(offset) {
  return source.readUInt16BE(offset);
}

function u32(offset) {
  return source.readUInt32BE(offset);
}

function writeU32(buffer, offset, value) {
  buffer.writeUInt32BE(value >>> 0, offset);
}

function align4(value) {
  return (value + 3) & ~3;
}

function checksum(buffer, start, length) {
  let sum = 0;
  const padded = align4(length);
  for (let i = 0; i < padded; i += 4) {
    const b0 = i < length ? buffer[start + i] : 0;
    const b1 = i + 1 < length ? buffer[start + i + 1] : 0;
    const b2 = i + 2 < length ? buffer[start + i + 2] : 0;
    const b3 = i + 3 < length ? buffer[start + i + 3] : 0;
    sum = (sum + (((b0 << 24) >>> 0) + (b1 << 16) + (b2 << 8) + b3)) >>> 0;
  }
  return sum >>> 0;
}

if (source.toString("ascii", 0, 4) !== "ttcf") {
  console.error(`${input} is not a TTC file`);
  process.exit(1);
}

const fontCount = u32(8);
if (index < 0 || index >= fontCount) {
  console.error(`font-index ${index} is out of range 0..${fontCount - 1}`);
  process.exit(1);
}

const fontOffset = u32(12 + index * 4);
const sfntVersion = u32(fontOffset);
const numTables = u16(fontOffset + 4);
const searchRange = u16(fontOffset + 6);
const entrySelector = u16(fontOffset + 8);
const rangeShift = u16(fontOffset + 10);

const records = [];
for (let i = 0; i < numTables; i++) {
  const pos = fontOffset + 12 + i * 16;
  records.push({
    tag: source.toString("ascii", pos, pos + 4),
    checkSum: u32(pos + 4),
    offset: u32(pos + 8),
    length: u32(pos + 12),
  });
}

let dataOffset = 12 + numTables * 16;
for (const record of records) {
  record.newOffset = dataOffset;
  dataOffset += align4(record.length);
}

const outputBuffer = Buffer.alloc(dataOffset);
writeU32(outputBuffer, 0, sfntVersion);
outputBuffer.writeUInt16BE(numTables, 4);
outputBuffer.writeUInt16BE(searchRange, 6);
outputBuffer.writeUInt16BE(entrySelector, 8);
outputBuffer.writeUInt16BE(rangeShift, 10);

for (let i = 0; i < records.length; i++) {
  const record = records[i];
  const pos = 12 + i * 16;
  outputBuffer.write(record.tag, pos, 4, "ascii");
  writeU32(outputBuffer, pos + 4, record.checkSum);
  writeU32(outputBuffer, pos + 8, record.newOffset);
  writeU32(outputBuffer, pos + 12, record.length);
  source.copy(outputBuffer, record.newOffset, record.offset, record.offset + record.length);
}

const head = records.find((record) => record.tag === "head");
if (head) {
  writeU32(outputBuffer, head.newOffset + 8, 0);
  const adjustment = (0xb1b0afba - checksum(outputBuffer, 0, outputBuffer.length)) >>> 0;
  writeU32(outputBuffer, head.newOffset + 8, adjustment);
}

fs.writeFileSync(output, outputBuffer);
console.log(`Extracted font ${index}/${fontCount - 1} to ${output}`);
