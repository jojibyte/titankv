'use strict'

/**
 * TitanKV – Real-World Example: Session Store + Leaderboard + Chat
 *
 * Demonstrates all major TitanKV features in a realistic scenario:
 * - Session management with TTL
 * - User profiles with Hashes
 * - Leaderboard with Sorted Sets
 * - Task queue with Lists
 * - Tag system with Sets
 * - Real-time chat with Pub/Sub
 * - Transactions for atomic updates
 * - JSON import/export
 * - Statistics monitoring
 */

const { TitanKV } = require('titankv')

const db = new TitanKV()

// ─── 1. Session Store (TTL) ───
console.log('\n=== Session Store ===')

function createSession(userId, ttlMs) {
  if (!userId) throw new Error('userId required')
  if (!ttlMs) throw new Error('ttlMs required')
  const token = `sess_${Date.now()}_${Math.random().toString(36).slice(2)}`
  db.put(`session:${token}`, userId, ttlMs)
  db.put(`user_session:${userId}`, token)
  return token
}

function getSession(token) {
  if (!token) throw new Error('token required')
  if (!db.has(`session:${token}`)) return null
  return db.get(`session:${token}`)
}

const token = createSession('user:alice', 3600000) // 1 hour
console.log(`  Created session: ${token}`)
console.log(`  Session user: ${getSession(token)}`)
console.log(`  TTL remaining: ${db.ttl(`session:${token}`)}ms`)

// ─── 2. User Profiles (Hashes) ───
console.log('\n=== User Profiles ===')

const users = [
  { id: 'alice', name: 'Alice', age: '28', city: 'Istanbul', role: 'admin' },
  { id: 'bob', name: 'Bob', age: '32', city: 'Berlin', role: 'user' },
  { id: 'charlie', name: 'Charlie', age: '25', city: 'Tokyo', role: 'user' },
]

for (const u of users) {
  db.hmset(`profile:${u.id}`, u)
}

const aliceProfile = db.hgetall('profile:alice')
console.log(`  Alice: ${JSON.stringify(aliceProfile)}`)
console.log(`  Bob's city: ${db.hget('profile:bob', 'city')}`)
console.log(`  Charlie's fields: ${db.hkeys('profile:charlie').join(', ')}`)

// Birthday! Increment age
db.hincrby('profile:alice', 'age', 1)
console.log(`  Alice's new age: ${db.hget('profile:alice', 'age')}`)

// ─── 3. Leaderboard (Sorted Sets) ───
console.log('\n=== Leaderboard ===')

const scores = [
  [1500, 'alice'], [2300, 'bob'], [1800, 'charlie'],
  [3100, 'diana'], [950, 'eve'], [2750, 'frank'],
]

db.zadd('leaderboard', ...scores.flat())

const top3 = db.zrevrange('leaderboard', 0, 2, { withScores: true })
console.log('  Top 3:')
top3.forEach((p, i) => console.log(`    ${i + 1}. ${p.member} — ${p.score} pts`))

// Alice gets a bonus
db.zincrby('leaderboard', 1000, 'alice')
console.log(`  Alice after bonus: rank #${db.zcard('leaderboard') - db.zrank('leaderboard', 'alice')}`)

// Players above 2000 points
const elite = db.zrangebyscore('leaderboard', 2000, '+inf', { withScores: true })
console.log(`  Elite players (>2000): ${elite.map(e => e.member).join(', ')}`)

// ─── 4. Task Queue (Lists) ───
console.log('\n=== Task Queue ===')

// Producer: add tasks
const tasks = ['send-email:alice', 'resize-image:42', 'process-payment:99', 'send-sms:bob']
for (const t of tasks) db.rpush('task:queue', t)
console.log(`  Queue length: ${db.llen('task:queue')}`)

// Consumer: process tasks
let processed = 0
const MAX_PROCESS = 10
let task = db.lpop('task:queue')
while (task && processed < MAX_PROCESS) {
  console.log(`  Processing: ${task}`)
  processed++
  task = db.lpop('task:queue')
}
console.log(`  Remaining: ${db.llen('task:queue')}`)

// ─── 5. Tag System (Sets) ───
console.log('\n=== Tag System ===')

db.sadd('tags:post:1', 'javascript', 'nodejs', 'database', 'performance')
db.sadd('tags:post:2', 'javascript', 'react', 'frontend')
db.sadd('tags:post:3', 'nodejs', 'database', 'backend')

console.log(`  Post 1 tags: ${db.smembers('tags:post:1').join(', ')}`)
console.log(`  Post 1 has 'nodejs': ${db.sismember('tags:post:1', 'nodejs')}`)
console.log(`  Post 2 tag count: ${db.scard('tags:post:2')}`)

// Remove a tag
db.srem('tags:post:1', 'performance')
console.log(`  Post 1 after remove: ${db.smembers('tags:post:1').join(', ')}`)

// ─── 6. Chat with Pub/Sub ───
console.log('\n=== Pub/Sub Chat ===')

const chatLog = []

db.subscribe('chat:general', (msg, channel) => {
  chatLog.push({ channel, msg })
})

db.subscribe('chat:*', (msg, channel) => {
  // pattern subscriber logs all channels
})

db.publish('chat:general', 'Hello everyone!')
db.publish('chat:general', 'TitanKV is fast!')
db.publish('chat:private', 'Secret message')

console.log(`  Messages in #general: ${chatLog.length}`)
chatLog.forEach(m => console.log(`    [${m.channel}] ${m.msg}`))

db.unsubscribe('chat:general')

// ─── 7. Atomic Transaction ───
console.log('\n=== Transaction ===')

const tx = db.multi()
tx.put('transfer:from', '900')   // deduct
tx.put('transfer:to', '1100')    // credit
tx.incr('transfer:count')
tx.sadd('transfer:log', `${Date.now()}:100`)
const results = tx.exec()
console.log(`  Transaction results: [${results.join(', ')}]`)
console.log(`  From balance: ${db.get('transfer:from')}`)
console.log(`  To balance: ${db.get('transfer:to')}`)

// ─── 8. Counters & Analytics ───
console.log('\n=== Counters ===')

const pages = ['/', '/about', '/products', '/contact', '/', '/', '/products']
for (const p of pages) db.incr(`pageview:${p}`)

console.log(`  Homepage views: ${db.get('pageview:/')}`)
console.log(`  Products views: ${db.get('pageview:/products')}`)

// ─── 9. Statistics ───
console.log('\n=== Database Stats ===')

const stats = db.stats()
console.log(`  Total ops:      ${stats.totalOps}`)
console.log(`  Keys:           ${stats.keyCount}`)
console.log(`  Hit rate:       ${(stats.hitRate * 100).toFixed(1)}%`)
console.log(`  Compression:    ${stats.compressionRatio.toFixed(2)}x`)

// ─── 10. Cleanup ───
db.close()
console.log('\n  Database closed. Done!\n')
