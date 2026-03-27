// Author: joji
'use strict';

const http = require('node:http');
const path = require('node:path');
const fs = require('node:fs');
const { URL } = require('node:url');
const { TitanKV } = require('../../lib');

const PORT = Number(process.env.PORT || 3030);
const HOST = '127.0.0.1';
const PUBLIC_DIR = path.join(__dirname, 'public');
const DATA_DIR = path.join(__dirname, 'data');

if (!fs.existsSync(DATA_DIR)) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
}

const db = new TitanKV(DATA_DIR, { sync: 'sync' });

function writeJson(res, status, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': Buffer.byteLength(body),
    'cache-control': 'no-store'
  });
  res.end(body);
}

function writeText(res, status, text) {
  res.writeHead(status, { 'content-type': 'text/plain; charset=utf-8' });
  res.end(text);
}

function readBody(req, maxBytes = 1024 * 1024) {
  return new Promise((resolve, reject) => {
    let size = 0;
    const chunks = [];
    req.on('data', (chunk) => {
      size += chunk.length;
      if (size > maxBytes) {
        reject(new Error('Payload too large'));
        req.destroy();
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

function validateKey(key) {
  return typeof key === 'string' && key.length > 0 && key.length <= 512;
}

function serveStatic(reqPath, res) {
  const safePath = reqPath === '/' ? '/index.html' : reqPath;
  const normalized = path.normalize(safePath).replace(/^\.+/, '');
  const filePath = path.join(PUBLIC_DIR, normalized);

  if (!filePath.startsWith(PUBLIC_DIR)) {
    writeText(res, 403, 'Forbidden');
    return;
  }

  if (!fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
    writeText(res, 404, 'Not found');
    return;
  }

  const ext = path.extname(filePath).toLowerCase();
  const type = ext === '.html'
    ? 'text/html; charset=utf-8'
    : ext === '.js'
      ? 'application/javascript; charset=utf-8'
      : ext === '.css'
        ? 'text/css; charset=utf-8'
        : 'application/octet-stream';

  res.writeHead(200, { 'content-type': type });
  fs.createReadStream(filePath).pipe(res);
}

async function handleApi(req, res, urlObj) {
  try {
    if (req.method === 'GET' && urlObj.pathname === '/api/health') {
      writeJson(res, 200, { ok: true, size: db.size() });
      return;
    }

    if (req.method === 'GET' && urlObj.pathname === '/api/keys') {
      const prefix = urlObj.searchParams.get('prefix') || '';
      const entries = db.scan(prefix, 200);
      const keys = entries.map(([key]) => key);
      writeJson(res, 200, { keys });
      return;
    }

    if (req.method === 'GET' && urlObj.pathname === '/api/get') {
      const key = urlObj.searchParams.get('key') || '';
      if (!validateKey(key)) {
        writeJson(res, 400, { error: 'Invalid key' });
        return;
      }
      const value = db.get(key);
      writeJson(res, 200, { key, value });
      return;
    }

    if (req.method === 'POST' && urlObj.pathname === '/api/put') {
      const raw = await readBody(req);
      const payload = raw ? JSON.parse(raw) : {};
      const key = payload.key;
      const value = payload.value;
      const ttlMs = payload.ttlMs;

      if (!validateKey(key) || typeof value !== 'string') {
        writeJson(res, 400, { error: 'Invalid key or value' });
        return;
      }

      const ttl = Number.isFinite(ttlMs) && ttlMs > 0 ? Math.floor(ttlMs) : 0;
      db.put(key, value, ttl);
      writeJson(res, 200, { ok: true, key });
      return;
    }

    if (req.method === 'DELETE' && urlObj.pathname === '/api/key') {
      const key = urlObj.searchParams.get('key') || '';
      if (!validateKey(key)) {
        writeJson(res, 400, { error: 'Invalid key' });
        return;
      }

      const deleted = db.del(key);
      writeJson(res, 200, { ok: true, deleted });
      return;
    }

    writeJson(res, 404, { error: 'Not found' });
  } catch (err) {
    writeJson(res, 500, { error: err.message || 'Internal error' });
  }
}

const server = http.createServer(async (req, res) => {
  const urlObj = new URL(req.url, `http://${HOST}:${PORT}`);

  if (urlObj.pathname.startsWith('/api/')) {
    await handleApi(req, res, urlObj);
    return;
  }

  serveStatic(urlObj.pathname, res);
});

server.listen(PORT, HOST, () => {
  console.log(`Frontend test server: http://${HOST}:${PORT}`);
});

function shutdown() {
  try {
    db.close();
  } catch (_) {}
  server.close(() => process.exit(0));
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
