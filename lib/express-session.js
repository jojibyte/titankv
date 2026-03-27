const debug = require('debug')('titankv:express-session')

module.exports = function (session) {
  if (!session) throw new Error('express-session module is required')
  if (!session.Store) throw new Error('invalid express-session module')

  const Store = session.Store

  class TitanKVSessionStore extends Store {
    constructor(options) {
      const opts = options || {}
      super(opts)

      if (!opts.client) throw new Error('TitanKV client instance is required in options')

      this.client = opts.client
      this.prefix = opts.prefix == null ? 'sess:' : opts.prefix
      this.ttlMs = opts.ttlMs || 86400000 // 1 day default

      debug('TitanKVSessionStore initialized with prefix %s', this.prefix)
    }

    get(sid, cb) {
      if (!sid) return cb(new Error('session id required'))
      try {
        debug('GET %s', sid)
        const val = this.client.get(this.prefix + sid)
        if (!val) return cb(null, null)
        return cb(null, JSON.parse(val))
      } catch (err) {
        debug('GET error: %s', err.message)
        cb(err)
      }
    }

    set(sid, sess, cb) {
      if (!sid) return cb && cb(new Error('session id required'))
      if (!sess) return cb && cb(new Error('session data required'))

      try {
        let ttl = this.ttlMs
        if (sess && sess.cookie && sess.cookie.expires) {
          const ms = new Date(sess.cookie.expires) - new Date()
          ttl = Math.max(Math.floor(ms), 0)
        } else if (sess && sess.cookie && typeof sess.cookie.maxAge === 'number') {
          ttl = sess.cookie.maxAge
        }

        debug('SET %s (ttl: %d ms)', sid, ttl)
        this.client.put(this.prefix + sid, JSON.stringify(sess), ttl)
        if (cb) cb(null)
      } catch (err) {
        debug('SET error: %s', err.message)
        if (cb) cb(err)
      }
    }

    destroy(sid, cb) {
      if (!sid) return cb && cb(new Error('session id required'))
      try {
        debug('DESTROY %s', sid)
        this.client.del(this.prefix + sid)
        if (cb) cb(null)
      } catch (err) {
        debug('DESTROY error: %s', err.message)
        if (cb) cb(err)
      }
    }

    touch(sid, sess, cb) {
      if (!sid) return cb && cb(new Error('session id required'))
      if (!sess) return cb && cb(new Error('session data required'))

      try {
        debug('TOUCH %s', sid)
        let ttl = this.ttlMs
        if (sess && sess.cookie && typeof sess.cookie.maxAge === 'number') {
          ttl = sess.cookie.maxAge
        }
        this.client.expire(this.prefix + sid, ttl)
        if (cb) cb(null)
      } catch (err) {
        debug('TOUCH error: %s', err.message)
        if (cb) cb(err)
      }
    }

    clear(cb) {
      try {
        debug('CLEAR')
        const keys = this.client.scan(this.prefix, 1000000)
        for (let i = 0; i < keys.length; i++) {
            this.client.del(keys[i][0])
        }
        if (cb) cb(null)
      } catch (err) {
        debug('CLEAR error: %s', err.message)
        if (cb) cb(err)
      }
    }

    length(cb) { 
      try {
        debug('LENGTH')
        const count = this.client.countPrefix(this.prefix)
        if (cb) cb(null, count)
      } catch (err) {
        debug('LENGTH error: %s', err.message)
        if (cb) cb(err)
      }
    }
  }

  return TitanKVSessionStore
}
