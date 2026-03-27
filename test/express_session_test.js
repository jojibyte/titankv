const { TitanKV } = require('../lib')
const initSessionStore = require('../lib/express-session')

// express-session modülünü mockluyoruz (Test ortamı için)
const EventEmitter = require('events')
class MockStore extends EventEmitter {}
const mockSession = { Store: MockStore }

// TitanKV Store uyarlamasını başlatıyoruz
const TitanKVStore = initSessionStore(mockSession)
const db = new TitanKV()
const store = new TitanKVStore({ client: db, prefix: 'extest:' })

let passed = 0
let failed = 0

function test(name, condition) {
  if (condition) {
    console.log(`  \u2713 ${name}`)
    passed++
  } else {
    console.error(`  \u2717 ${name}`)
    failed++
  }
}

;(function runTests() {
    console.log('╔═══════════════════════════════════════════════════════════╗')
    console.log('║  TitanKV Phase 2: Express Session Store Integration Test  ║')
    console.log('╚═══════════════════════════════════════════════════════════╝\n')

    store.set('sid_1', { user: 'alice', cookie: { maxAge: 5000 } }, (err) => {
        test('set returns no error', !err)
        test('key inserted in db', !!db.get('extest:sid_1'))
        test('ttl applied correctly', db.ttl('extest:sid_1') <= 5000 && db.ttl('extest:sid_1') > 0)

        store.get('sid_1', (err, sess) => {
            test('get returns no error', !err)
            test('get returns correct data', sess && sess.user === 'alice')

            store.length((err, len) => {
                test('length is 1', len === 1)

                store.touch('sid_1', { user: 'alice', cookie: { maxAge: 10000 } }, (err) => {
                    test('touch returns no error', !err)
                    test('touch updated ttl', db.ttl('extest:sid_1') > 5000)

                    store.destroy('sid_1', (err) => {
                        test('destroy returns no error', !err)
                        test('key deleted from db', !db.get('extest:sid_1'))

                        console.log(`\n  Results: ${passed} passed, ${failed} failed`)
                        db.close()
                        if (failed > 0) process.exit(1)
                    })
                })
            })
        })
    })
})()
