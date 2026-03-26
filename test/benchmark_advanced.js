'use strict'

const { TitanKV } = require('../lib')
const path = require('path')
const fs = require('fs')

const N = 100000

function fmt(n) { return n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',') }
function fmtB(b) {
  if (b < 1024) return b + ' B'
  if (b < 1024 * 1024) return (b / 1024).toFixed(1) + ' KB'
  return (b / (1024 * 1024)).toFixed(2) + ' MB'
}

function bench(label, fn) {
  if (!label) throw new Error('bench: label required')
  if (!fn) throw new Error('bench: fn required')
  const t0 = process.hrtime.bigint()
  const r = fn()
  const ms = Number(process.hrtime.bigint() - t0) / 1e6
  const ops = Math.round((r.ops / ms) * 1000)
  console.log(`  ${label.padEnd(36)} ${String(ms.toFixed(1) + 'ms').padStart(10)}  ${fmt(ops).padStart(14)} ops/s`)
  return { ms, ops, ...r }
}

function section(title) {
  console.log(`\n\u250C${'─'.repeat(65)}\u2510`)
  console.log(`\u2502 ${title.padEnd(64)}\u2502`)
  console.log(`\u2514${'─'.repeat(65)}\u2518`)
}

console.log('\u2554' + '═'.repeat(67) + '\u2557')
console.log('\u2551' + '  TitanKV Advanced Benchmark Suite v2.1.0'.padEnd(67) + '\u2551')
console.log('\u255A' + '═'.repeat(67) + '\u255D')

// ─── Mixed Read/Write Workload ───
section('Mixed Workload (80% read, 20% write)')

const mixDb = new TitanKV()
// seed 10K keys
const SEED = 10000
for (let i = 0; i < SEED; i++) mixDb.put(`mix:${i}`, `val-${i}`)

bench(`mixed ops (${fmt(N)})`, () => {
  const limit = N
  for (let i = 0; i < limit; i++) {
    if (i % 5 === 0) {
      mixDb.put(`mix:${i % SEED}`, `upd-${i}`)
    } else {
      mixDb.get(`mix:${i % SEED}`)
    }
  }
  return { ops: N }
})

// ─── Hot Key Access Pattern ───
section('Hot Key Access (Zipf-like)')

const hotDb = new TitanKV()
for (let i = 0; i < 1000; i++) hotDb.put(`hot:${i}`, 'x'.repeat(200))

bench(`hot key reads (${fmt(N)})`, () => {
  const limit = N
  for (let i = 0; i < limit; i++) {
    // top 10 keys get 80% of reads
    const k = Math.random() < 0.8 ? Math.floor(Math.random() * 10) : Math.floor(Math.random() * 1000)
    hotDb.get(`hot:${k}`)
  }
  return { ops: N }
})

// ─── TTL Stress Test ───
section('TTL Stress Test')

const ttlDb = new TitanKV()
const TTL_N = 50000

bench(`put with TTL (${fmt(TTL_N)})`, () => {
  const limit = TTL_N
  for (let i = 0; i < limit; i++) {
    ttlDb.put(`ttl:${i}`, `v-${i}`, 60000)
  }
  return { ops: TTL_N }
})

bench(`expire + ttl check (${fmt(TTL_N)})`, () => {
  const limit = TTL_N
  for (let i = 0; i < limit; i++) {
    ttlDb.expire(`ttl:${i}`, 30000)
    ttlDb.ttl(`ttl:${i}`)
  }
  return { ops: TTL_N * 2 }
})

// ─── Transaction Throughput ───
section('Transaction (MULTI/EXEC)')

const txDb = new TitanKV()
const TX_N = 10000
const TX_SIZE = 10

bench(`${fmt(TX_N)} transactions (${TX_SIZE} ops each)`, () => {
  const limit = TX_N
  for (let i = 0; i < limit; i++) {
    const tx = txDb.multi()
    const base = i * TX_SIZE
    for (let j = 0; j < TX_SIZE; j++) {
      tx.put(`tx:${base + j}`, `v-${base + j}`)
    }
    tx.exec()
  }
  return { ops: TX_N * TX_SIZE }
})

// ─── Pub/Sub Throughput ───
section('Pub/Sub Throughput')

const pubDb = new TitanKV()
let msgCount = 0
const PUB_N = 100000

pubDb.subscribe('bench:channel', () => { msgCount++ })

bench(`publish ${fmt(PUB_N)} messages`, () => {
  const limit = PUB_N
  for (let i = 0; i < limit; i++) {
    pubDb.publish('bench:channel', `msg-${i}`)
  }
  return { ops: PUB_N }
})

console.log(`  Messages received:                 ${fmt(msgCount)}`)
pubDb.unsubscribe('bench:channel')

// ─── Large Value Performance ───
section('Large Value Performance')

const lgDb = new TitanKV()
const sizes = [100, 1024, 10240, 102400]

for (const sz of sizes) {
  const val = 'A'.repeat(sz)
  const LG_N = Math.min(10000, Math.floor(500000 / sz))

  bench(`put ${fmtB(sz)} x ${fmt(LG_N)}`, () => {
    for (let i = 0; i < LG_N; i++) lgDb.put(`lg${sz}:${i}`, val)
    return { ops: LG_N }
  })

  bench(`get ${fmtB(sz)} x ${fmt(LG_N)}`, () => {
    for (let i = 0; i < LG_N; i++) lgDb.get(`lg${sz}:${i}`)
    return { ops: LG_N }
  })
}

// ─── Scan & Query Performance ───
section('Scan & Query Performance')

const scanDb = new TitanKV()
for (let i = 0; i < 50000; i++) scanDb.put(`scan:${String(i).padStart(6, '0')}`, `v-${i}`)

bench('scan prefix (50K keys)', () => {
  scanDb.scan('scan:', 1000)
  return { ops: 1000 }
})

bench('countPrefix (50K keys)', () => {
  const c = scanDb.countPrefix('scan:')
  return { ops: c }
})

bench('keysMatch glob (50K keys)', () => {
  scanDb.keysMatch('scan:000*')
  return { ops: 1 }
})

bench('range query (50K keys)', () => {
  scanDb.range('scan:010000', 'scan:020000', 1000)
  return { ops: 1000 }
})

// ─── Summary Stats ───
section('Final Stats')

const finalStats = scanDb.stats()
console.log(`  Total ops:         ${fmt(finalStats.totalOps)}`)
console.log(`  Keys:              ${fmt(finalStats.keyCount)}`)
console.log(`  Raw size:          ${fmtB(finalStats.rawBytes)}`)
console.log(`  Compressed:        ${fmtB(finalStats.compressedBytes)}`)
if (finalStats.rawBytes > 0) {
  console.log(`  Ratio:             ${(finalStats.rawBytes / finalStats.compressedBytes).toFixed(1)}x`)
}

console.log('\n\u2554' + '═'.repeat(67) + '\u2557')
console.log('\u2551' + '  Advanced Benchmark Complete'.padEnd(67) + '\u2551')
console.log('\u255A' + '═'.repeat(67) + '\u255D\n')
