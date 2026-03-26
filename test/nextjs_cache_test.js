const { TitanKV } = require('../lib')
const NextJSTitanKVCache = require('../lib/nextjs-cache')

const db = new TitanKV()
const cache = new NextJSTitanKVCache({ client: db, prefix: 'extest:' })

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

;(async function runTests() {
  console.log('╔═══════════════════════════════════════════════════════════╗')
  console.log('║   TitanKV Phase 2: Next.js Cache Handler Integration      ║')
  console.log('╚═══════════════════════════════════════════════════════════╝\n')

  try {
    // 1. Set without Revalidation
    await cache.set('fetch_page1', { html: '<h1>Hello</h1>' }, {})
    test('Set object saves in db', !!db.get('extest:fetch_page1'))

    const missed = await cache.get('invalid_key')
    test('Get invalid key returns null', missed === null)

    const hit = await cache.get('fetch_page1')
    test('Get valid key returns object', hit !== null && hit.html === '<h1>Hello</h1>')

    // 2. Set with Tags and TTL/Revalidation (seconds)
    await cache.set('api_data_2', { products: 123 }, { revalidate: 60, tags: ['products', 'api'] })

    // Check Tag Mapping
    test('Tag products is added to set', db.sismember('extest:tags:products', 'api_data_2'))
    test('Tag api is added to set', db.sismember('extest:tags:api', 'api_data_2'))

    const ttlRem = db.ttl('extest:api_data_2')
    test('Revalidation converted to TTL ms correctly', ttlRem > 0 && ttlRem <= 60000)

    // 3. Revalidate Tags (Purgation)
    await cache.revalidateTag('products')

    // Check purge
    const dataAfterReval = await cache.get('api_data_2')
    test('Revalidate tag purges cached item', dataAfterReval === null)
    test('Revalidate tag purges tag set', db.scard('extest:tags:products') === 0)

  } catch (err) {
    console.error('\nTest crashed:', err)
    failed++
  } finally {
    console.log(`\n  Results: ${passed} passed, ${failed} failed`)
    db.close()
    if (failed > 0) process.exit(1)
  }
})()
