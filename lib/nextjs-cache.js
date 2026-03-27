const debug = require('debug')('titankv:nextjs-cache')

class NextJSTitanKVCache {
  /**
   * Next.js Custom Cache Handler implemented via TitanKV.
   * Enables caching Data Fetching and ISR payload in TitanKV naturally.
   *
   * @param {Object} options
   * @param {Object} options.client TitanKV instance
   * @param {string} options.prefix Prefix to separate Next.js cache from other keys (defaults to 'nextcache:')
   */
  constructor(options) {
    const opts = options || {}
    if (!opts.client) throw new Error('TitanKV client instance is required in options')

    this.client = opts.client
    this.prefix = opts.prefix == null ? 'nextcache:' : opts.prefix

    debug('NextJSTitanKVCache initialized with prefix %s', this.prefix)
  }

  /**
   * Required by Next.js Handler API
   * Gets a cached item. Returning null/undefined means cache miss.
   */
  async get(key) {
    if (!key) return null
    try {
      debug('GET %s', key)
      const data = this.client.get(this.prefix + key)
      if (!data) return null
      return JSON.parse(data)
    } catch (err) {
      debug('GET error for key %s: %s', key, err.message)
      return null
    }
  }

  /**
   * Required by Next.js Handler API
   * Sets a cache item.
   * ctx.revalidate is expected in seconds. TitanKV expects MS.
   */
  async set(key, data, ctx) {
    if (!key || !data) return
    try {
      // ctx.revalidate is strictly in seconds for Next.js 14+
      // It can also be false or undefined for infinite caching.
      let ttlMs = 0 // 0 means no TTL limit in TitanKV
      let reval = ctx && ctx.revalidate != null ? ctx.revalidate : -1

      if (typeof reval === 'number' && reval > 0) {
        ttlMs = reval * 1000 // Convert sec to ms
      }

      debug('SET %s (ttl: %d ms)', key, ttlMs)
      this.client.put(this.prefix + key, JSON.stringify(data), ttlMs)

      // Store ISR/tag linking metadata if provided
      if (ctx && ctx.tags && Array.isArray(ctx.tags)) {
        for (const tag of ctx.tags) {
           this.client.sadd(this.prefix + 'tags:' + tag, key)
        }
      }
    } catch (err) {
      debug('SET error for key %s: %s', key, err.message)
    }
  }

  /**
   * Required by Next.js Handler API for revalidateTag()
   * Revalidates tags by identifying linked keys and purging them.
   */
  async revalidateTag(tag) {
    if (!tag) return
    try {
      debug('REVALIDATE TAG %s', tag)
      const tagKey = this.prefix + 'tags:' + tag
      const cacheKeys = this.client.smembers(tagKey)

      for (const k of cacheKeys) {
        this.client.del(this.prefix + k)
      }
      // Also delete the tag index itself
      this.client.del(tagKey)
    } catch (err) {
      debug('REVALIDATE TAG error for %s: %s', tag, err.message)
    }
  }
}

module.exports = NextJSTitanKVCache
