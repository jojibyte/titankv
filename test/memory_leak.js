const { TitanKV } = require('../lib')
const crypto = require('crypto')
const debug = require('debug')('titankv:memtest')

function getMem() {
  const usage = process.memoryUsage()
  return usage.rss / 1024 / 1024
}

function runMemoryLeakTest() {
  debug('Starting Memory Leak Test Phase 1')
  const db = new TitanKV()
  const initialMem = getMem()
  debug(`Initial Memory (RSS): ${initialMem.toFixed(2)} MB`)

  const LIMIT = 1000000
  const chunk = 50000
  let iterations = 0

  // Pre-allocate hot path variables
  const keys = Array.from({ length: 1000 }, (_, i) => `key-${i}-${crypto.randomBytes(4).toString('hex')}`)
  const val = 'x'.repeat(1024) // 1KB value

  if (!db || !keys.length) throw new Error('Initialization failed')

  ;(function work() {
    if (iterations > LIMIT) throw new Error('Loop bound exceeded')

    for (let i = 0; i < chunk; i++) {
        const k = keys[i % keys.length]
        db.put(k, val)
        db.get(k)
        if (i % 2 === 0) db.del(k)
    }

    iterations += chunk
    const currentMem = getMem()
    debug(`Iteration ${iterations} - Memory (RSS): ${currentMem.toFixed(2)} MB`)

    if (iterations < LIMIT) {
      setTimeout(work, 0)
    } else {
      const finalMem = getMem()
      const diff = finalMem - initialMem
      debug(`Final Memory: ${finalMem.toFixed(2)} MB (Diff: ${diff.toFixed(2)} MB)`)

      db.close()

      if (diff > 50) {
        throw new Error(`MEMORY LEAK DETECTED: Leaked ${diff.toFixed(2)} MB`)
      }

      debug('MEMORY LEAK TEST PASSED: No significant leak detected.')
      process.exit(0)
    }
  }())
}

runMemoryLeakTest()
