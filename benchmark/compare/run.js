// Author: joji
'use strict';

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

function envInt(name, fallback, min) {
  const raw = process.env[name];
  if (raw == null || raw === '') return fallback;
  const n = Number(raw);
  if (!Number.isFinite(n)) return fallback;
  const int = Math.floor(n);
  return int >= min ? int : fallback;
}

function envNum(name, fallback, min) {
  const raw = process.env[name];
  if (raw == null || raw === '') return fallback;
  const n = Number(raw);
  if (!Number.isFinite(n)) return fallback;
  return n >= min ? n : fallback;
}

const ITERATIONS = envInt('COMPARE_ITER', 20000, 1);
const WARMUP = envInt('COMPARE_WARMUP', 2000, 0);
const ROUNDS = envInt('COMPARE_ROUNDS', 5, 1);
const BURNIN_ROUNDS = envInt('COMPARE_BURNIN_ROUNDS', 1, 0);
const MAX_CV = envNum('COMPARE_MAX_CV', 0, 0);
const TOTAL_ROUNDS = ROUNDS + BURNIN_ROUNDS;
const HOT_KEY_SPACE = 1000;
const HOT_KEY_TOP = 10;
const LARGE_VALUE_BYTES = 16 * 1024;
const LARGE_VALUE_COUNT = Math.max(200, Math.floor(ITERATIONS / 20));
const DATA_ROOT = path.join(__dirname, '.data');
const LOCAL_REPORT_PATH = path.join(__dirname, 'benchmark-report.html');
const REPORTS_DIR = path.join(__dirname, 'reports');

const OP_ROWS = [
  { id: 'put', title: () => `insert ${fmtNum(ITERATIONS)} rows` },
  { id: 'get', title: () => `read ${fmtNum(ITERATIONS)} rows` },
  { id: 'miss', title: () => `read ${fmtNum(ITERATIONS)} misses` },
  { id: 'mixed', title: () => `mixed 80% read / 20% write (${fmtNum(ITERATIONS)} ops)` },
  { id: 'hot', title: () => `hot-key read (${fmtNum(ITERATIONS)} ops, ${HOT_KEY_TOP}/${HOT_KEY_SPACE} skew)` },
  { id: 'largePut', title: () => `put ${fmtNum(LARGE_VALUE_COUNT)} values (${fmtNum(LARGE_VALUE_BYTES)} bytes each)` },
  { id: 'largeGet', title: () => `get ${fmtNum(LARGE_VALUE_COUNT)} values (${fmtNum(LARGE_VALUE_BYTES)} bytes each)` },
];

function fmtNum(n) {
  return Number(n).toLocaleString('en-US');
}

function fmtMs(n) {
  return `${n.toFixed(1)} ms`;
}

function fmtOps(n) {
  return `${fmtNum(Math.round(n))} ops/s`;
}

function createDeterministicRandom(seedBase) {
  let seed = seedBase >>> 0;
  return function next() {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    return seed / 4294967296;
  };
}

function median(values) {
  if (!values || values.length === 0) return 0;
  const sorted = values.slice().sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  if (sorted.length % 2 === 0) {
    return (sorted[mid - 1] + sorted[mid]) / 2;
  }
  return sorted[mid];
}

function mean(values) {
  if (!values || values.length === 0) return 0;
  return values.reduce((acc, n) => acc + n, 0) / values.length;
}

function coefficientOfVariation(values) {
  if (!values || values.length <= 1) return 0;
  const m = mean(values);
  if (m === 0) return 0;
  const variance = values.reduce((acc, n) => acc + ((n - m) ** 2), 0) / values.length;
  const stddev = Math.sqrt(variance);
  return (stddev / m) * 100;
}

function getSystemInfo() {
  const cpus = os.cpus() || [];
  const cpuModel = cpus[0] ? cpus[0].model.trim() : 'unknown';
  return {
    cpuModel,
  };
}

function stageFileName(index, key) {
  return `${String(index).padStart(2, '0')}-${key}.html`;
}

function buildStageLabel(results) {
  const active = results.filter((r) => !r.skipped).map((r) => r.label);
  return active.length > 0 ? active.join(' + ') : 'none';
}

function optionalRequire(name) {
  try {
    return { ok: true, mod: require(name) };
  } catch (error) {
    return { ok: false, error };
  }
}

function removeDir(target) {
  try {
    fs.rmSync(target, { recursive: true, force: true });
  } catch {
    // ignore
  }
}

function ensureDir(target) {
  fs.mkdirSync(target, { recursive: true });
}

function hrtimeMs(start) {
  return Number(process.hrtime.bigint() - start) / 1e6;
}

function scoreClass(slowdown) {
  if (slowdown <= 1.15) return 'fast';
  if (slowdown <= 1.5) return 'mid';
  return 'slow';
}

function escapeHtml(v) {
  return String(v)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function sanitizeErrorMessage(message) {
  if (!message) return 'unknown error';

  const text = String(message)
    // Mask Windows absolute paths (drive letter + backslash pattern).
    .replace(/[A-Za-z]:\\[^\r\n)]+/g, '<local-path>')
    // Mask POSIX absolute paths (leading forward slash pattern).
    .replace(/\/(?:[^\s)]+\/)+[^\s)]+/g, '<local-path>')
    // Normalize mojibake that can appear in localized error outputs.
    .replace(/Eri�im/g, 'Erisim')
    .replace(/[\r\n]+/g, ' ')
    .trim();

  return text.length > 300 ? `${text.slice(0, 297)}...` : text;
}

function isNotFoundError(err) {
  if (!err) return false;
  const msg = String(err.message || err || '');
  return Boolean(err.notFound || err.code === 'LEVEL_NOT_FOUND' || /not\s*found/i.test(msg));
}

async function createTitanEngine(baseDir) {
  const { TitanKV } = require('../../lib');
  const dir = path.join(baseDir, 'titankv');
  ensureDir(dir);
  const db = new TitanKV(dir, { sync: 'sync' });
  return {
    put: (k, v) => db.put(k, v),
    get: (k) => db.get(k),
    close: () => db.close(),
  };
}

async function createSqliteEngine(baseDir) {
  const loaded = optionalRequire('better-sqlite3');
  if (!loaded.ok) {
    throw new Error('missing module better-sqlite3');
  }

  const Database = loaded.mod;
  const dbPath = path.join(baseDir, 'sqlite.db');
  const db = new Database(dbPath);
  db.pragma('journal_mode = WAL');
  db.pragma('synchronous = NORMAL');
  db.exec('CREATE TABLE IF NOT EXISTS kv (k TEXT PRIMARY KEY, v TEXT NOT NULL)');
  const putStmt = db.prepare('INSERT INTO kv (k, v) VALUES (?, ?) ON CONFLICT(k) DO UPDATE SET v=excluded.v');
  const getStmt = db.prepare('SELECT v FROM kv WHERE k = ?');

  return {
    put: (k, v) => putStmt.run(k, v),
    get: (k) => {
      const row = getStmt.get(k);
      return row ? row.v : null;
    },
    close: () => db.close(),
  };
}

async function createLmdbEngine(baseDir) {
  const loaded = optionalRequire('lmdb');
  if (!loaded.ok) {
    throw new Error('missing module lmdb');
  }

  const db = loaded.mod.open({
    path: path.join(baseDir, 'lmdb'),
    compression: false,
  });

  return {
    put: (k, v) => db.putSync(k, v),
    get: (k) => db.get(k),
    close: () => db.close(),
  };
}

async function createLevelEngine(baseDir) {
  const loaded = optionalRequire('level');
  if (!loaded.ok) {
    throw new Error('missing module level');
  }

  const { Level } = loaded.mod;
  const db = new Level(path.join(baseDir, 'level'), { valueEncoding: 'utf8' });

  return {
    put: (k, v) => db.put(k, v),
    get: (k) => db.get(k).catch((err) => {
      if (isNotFoundError(err)) return null;
      throw err;
    }),
    close: () => db.close(),
  };
}

async function createRocksDbEngine(baseDir) {
  const loaded = optionalRequire('rocksdb');
  if (!loaded.ok) {
    throw new Error('missing module rocksdb');
  }

  const rocksdb = loaded.mod;
  const dbPath = path.join(baseDir, 'rocksdb');
  const db = rocksdb(dbPath);

  await new Promise((resolve, reject) => {
    db.open({ createIfMissing: true }, (err) => (err ? reject(err) : resolve()));
  });

  return {
    put: (k, v) => new Promise((resolve, reject) => {
      db.put(k, v, (err) => (err ? reject(err) : resolve()));
    }),
    get: (k) => new Promise((resolve, reject) => {
      db.get(k, (err, value) => {
        if (isNotFoundError(err)) return resolve(null);
        if (err) return reject(err);
        return resolve(value);
      });
    }),
    close: () => new Promise((resolve, reject) => {
      db.close((err) => (err ? reject(err) : resolve()));
    }),
  };
}

const ENGINE_DEFS = [
  { key: 'titankv', label: 'titankv', create: createTitanEngine, install: 'already included' },
  { key: 'sqlite', label: 'better-sqlite3', create: createSqliteEngine, install: 'npm i better-sqlite3' },
  { key: 'lmdb', label: 'lmdb', create: createLmdbEngine, install: 'npm i lmdb' },
  { key: 'level', label: 'level', create: createLevelEngine, install: 'npm i level' },
  { key: 'rocksdb', label: 'rocksdb', create: createRocksDbEngine, install: 'npm i rocksdb' },
];

async function runEngineBenchmark(def) {
  const dataDir = path.join(DATA_ROOT, def.key);
  removeDir(dataDir);
  ensureDir(dataDir);

  const value = JSON.stringify({
    payload: 'x'.repeat(120),
    now: Date.now(),
    keyspace: def.key,
  });
  const largeValue = 'L'.repeat(LARGE_VALUE_BYTES);

  const roundsByOp = Object.fromEntries(OP_ROWS.map((r) => [r.id, []]));

  for (let round = 1; round <= TOTAL_ROUNDS; round++) {
    const roundDir = path.join(dataDir, `round-${round}`);
    removeDir(roundDir);
    ensureDir(roundDir);

    let engine;
    try {
      engine = await def.create(roundDir);
    } catch (error) {
      return {
        key: def.key,
        label: def.label,
        skipped: true,
        reason: sanitizeErrorMessage(error && error.message ? error.message : 'failed to initialize'),
        install: def.install,
      };
    }

    const warmKeys = [];
    for (let i = 0; i < WARMUP; i++) {
      warmKeys.push(`warm:${round}:${i}`);
    }

    const putKeys = [];
    for (let i = 0; i < ITERATIONS; i++) {
      putKeys.push(`k:${round}:${i}`);
    }

    const hotKeys = [];
    for (let i = 0; i < HOT_KEY_SPACE; i++) {
      hotKeys.push(`hot:${round}:${i}`);
    }

    const largeKeys = [];
    for (let i = 0; i < LARGE_VALUE_COUNT; i++) {
      largeKeys.push(`lg:${round}:${i}`);
    }

    try {
      for (const k of warmKeys) {
        await Promise.resolve(engine.put(k, value));
        await Promise.resolve(engine.get(k));
      }

      const putStart = process.hrtime.bigint();
      for (const k of putKeys) {
        await Promise.resolve(engine.put(k, value));
      }
      const putMs = hrtimeMs(putStart);

      const getStart = process.hrtime.bigint();
      for (const k of putKeys) {
        await Promise.resolve(engine.get(k));
      }
      const getMs = hrtimeMs(getStart);

      const missStart = process.hrtime.bigint();
      for (let i = 0; i < ITERATIONS; i++) {
        await Promise.resolve(engine.get(`missing:${round}:${i}`));
      }
      const missMs = hrtimeMs(missStart);

      const mixedStart = process.hrtime.bigint();
      for (let i = 0; i < ITERATIONS; i++) {
        const k = putKeys[i % putKeys.length];
        if (i % 5 === 0) {
          await Promise.resolve(engine.put(k, `upd:${round}:${i}`));
        } else {
          await Promise.resolve(engine.get(k));
        }
      }
      const mixedMs = hrtimeMs(mixedStart);

      for (const k of hotKeys) {
        await Promise.resolve(engine.put(k, value));
      }

      const rnd = createDeterministicRandom(round * 2654435761);
      const hotStart = process.hrtime.bigint();
      for (let i = 0; i < ITERATIONS; i++) {
        const idx = rnd() < 0.8
          ? Math.floor(rnd() * HOT_KEY_TOP)
          : Math.floor(rnd() * HOT_KEY_SPACE);
        await Promise.resolve(engine.get(hotKeys[idx]));
      }
      const hotMs = hrtimeMs(hotStart);

      const largePutStart = process.hrtime.bigint();
      for (const k of largeKeys) {
        await Promise.resolve(engine.put(k, largeValue));
      }
      const largePutMs = hrtimeMs(largePutStart);

      const largeGetStart = process.hrtime.bigint();
      for (const k of largeKeys) {
        await Promise.resolve(engine.get(k));
      }
      const largeGetMs = hrtimeMs(largeGetStart);

      if (round > BURNIN_ROUNDS) {
        roundsByOp.put.push(putMs);
        roundsByOp.get.push(getMs);
        roundsByOp.miss.push(missMs);
        roundsByOp.mixed.push(mixedMs);
        roundsByOp.hot.push(hotMs);
        roundsByOp.largePut.push(largePutMs);
        roundsByOp.largeGet.push(largeGetMs);
      }
    } catch (error) {
      return {
        key: def.key,
        label: def.label,
        skipped: true,
        reason: `runtime error: ${sanitizeErrorMessage(error && error.message ? error.message : 'unknown error')}`,
        install: def.install,
      };
    } finally {
      try {
        await Promise.resolve(engine.close());
      } catch {
        // ignore close errors
      }
    }
  }

  const metricsMs = {};
  const metricsOps = {};
  const metricsCv = {};
  for (const row of OP_ROWS) {
    const ms = median(roundsByOp[row.id]);
    metricsMs[row.id] = ms;
    const opCount = row.id === 'largePut' || row.id === 'largeGet' ? LARGE_VALUE_COUNT : ITERATIONS;
    metricsOps[row.id] = ms > 0 ? (opCount / ms) * 1000 : 0;
    metricsCv[row.id] = coefficientOfVariation(roundsByOp[row.id]);
  }

  return {
    key: def.key,
    label: def.label,
    skipped: false,
    rounds: ROUNDS,
    burninRounds: BURNIN_ROUNDS,
    totalRounds: TOTAL_ROUNDS,
    roundsByOp,
    metricsMs,
    metricsOps,
    metricsCv,
  };
}

function geometricMean(values) {
  if (values.length === 0) return 0;
  const sum = values.reduce((acc, n) => acc + Math.log(n), 0);
  return Math.exp(sum / values.length);
}

function toSlowdownRows(results) {
  const active = results.filter((r) => !r.skipped);
  if (active.length === 0) {
    return {
      renderedRows: [],
      gmRow: {
        title: 'weighted geometric mean',
        cells: [],
      },
      throughputRows: [],
    };
  }

  const rows = OP_ROWS.map((op) => {
    const fastest = Math.min(...active.map((r) => r.metricsMs[op.id]));
    return {
      id: op.id,
      title: op.title(),
      fastest,
      getMs: (r) => r.metricsMs[op.id],
      getOps: (r) => r.metricsOps[op.id],
    };
  });

  const valuesByEngine = new Map();
  for (const r of active) {
    valuesByEngine.set(r.key, []);
  }

  const renderedRows = rows.map((row) => {
    const cells = active.map((r) => {
      const ms = row.getMs(r);
      const slowdown = ms / row.fastest;
      valuesByEngine.get(r.key).push(slowdown);
      return {
        key: r.key,
        ms,
        slowdown,
        cls: scoreClass(slowdown),
      };
    });

    return {
      title: row.title,
      cells,
    };
  });

  const throughputRows = rows.map((row) => {
    const cells = active.map((r) => {
      return {
        key: r.key,
        ops: row.getOps(r),
      };
    });
    return {
      title: row.title,
      cells,
    };
  });

  const gmRow = {
    title: 'weighted geometric mean',
    cells: active.map((r) => {
      const gm = geometricMean(valuesByEngine.get(r.key));
      return {
        key: r.key,
        gm,
        cls: scoreClass(gm),
      };
    }),
  };

  return { renderedRows, gmRow, throughputRows };
}

function buildSummaryRows(active, gmRow) {
  const gmMap = new Map(gmRow.cells.map((c) => [c.key, c.gm]));
  const sorted = active.slice().sort((a, b) => (gmMap.get(a.key) || Infinity) - (gmMap.get(b.key) || Infinity));
  return sorted.map((r, idx) => {
    return {
      rank: idx + 1,
      label: r.label,
      gm: gmMap.get(r.key) || 0,
      putMs: r.metricsMs.put,
      getMs: r.metricsMs.get,
      missMs: r.metricsMs.miss,
      mixedMs: r.metricsMs.mixed,
      hotMs: r.metricsMs.hot,
      largePutMs: r.metricsMs.largePut,
      largeGetMs: r.metricsMs.largeGet,
      stabilityCv: mean([
        r.metricsCv.put,
        r.metricsCv.get,
        r.metricsCv.miss,
        r.metricsCv.mixed,
        r.metricsCv.hot,
        r.metricsCv.largePut,
        r.metricsCv.largeGet,
      ]),
    };
  });
}

function renderHtml(results, startedAt, systemInfo, stageMeta = null) {
  const active = results.filter((r) => !r.skipped);
  const skipped = results.filter((r) => r.skipped);

  const { renderedRows, gmRow, throughputRows } = toSlowdownRows(results);
  const summaryRows = buildSummaryRows(active, gmRow);

  const headers = active.map((r) => `<th>${escapeHtml(r.label)}</th>`).join('');

  const rowHtml = renderedRows.map((row) => {
    const tds = row.cells.map((cell) => {
      return `<td class="${cell.cls}">${cell.ms.toFixed(1)} <small>(${cell.slowdown.toFixed(2)}x)</small></td>`;
    }).join('');

    return `<tr><th class="bench">${escapeHtml(row.title)}</th>${tds}</tr>`;
  }).join('');

  const gmCells = gmRow.cells.map((cell) => {
    return `<td class="${cell.cls}">${cell.gm.toFixed(2)}</td>`;
  }).join('');

  const stageHtml = stageMeta
    ? `<p class="mono"><strong>Stage ${stageMeta.index}/${stageMeta.total}</strong>: ${escapeHtml(stageMeta.label)}</p>`
    : '';

  const systemHtml = `<table class="meta mono">
      <tbody>
        <tr><th>CPU</th><td colspan="3">${escapeHtml(systemInfo.cpuModel)}</td></tr>
        <tr><th>Method</th><td colspan="3">median of ${fmtNum(ROUNDS)} measured rounds, ${fmtNum(BURNIN_ROUNDS)} burn-in rounds ignored, fresh data directory each round</td></tr>
      </tbody>
    </table>`;

  const summaryHtml = summaryRows.length === 0
    ? '<tr><td colspan="11">No completed engines yet.</td></tr>'
    : summaryRows.map((r) => {
      return `<tr>
        <td>${r.rank}</td>
        <td>${escapeHtml(r.label)}</td>
        <td>${r.gm.toFixed(2)}</td>
        <td>${fmtMs(r.putMs)}</td>
        <td>${fmtMs(r.getMs)}</td>
        <td>${fmtMs(r.missMs)}</td>
        <td>${fmtMs(r.mixedMs)}</td>
        <td>${fmtMs(r.hotMs)}</td>
        <td>${fmtMs(r.largePutMs)}</td>
        <td>${fmtMs(r.largeGetMs)}</td>
        <td>${r.stabilityCv.toFixed(1)}%</td>
      </tr>`;
    }).join('');

  const throughputHtml = throughputRows.map((row) => {
    const tds = row.cells.map((cell) => {
      return `<td>${fmtOps(cell.ops)}</td>`;
    }).join('');

    return `<tr><th class="bench">${escapeHtml(row.title)}</th>${tds}</tr>`;
  }).join('');

  const bodyRows = active.length === 0
    ? '<tr><th class="bench">status</th><td>No active engines completed in this stage.</td></tr>'
    : `${rowHtml}
          <tr>
            <th class="bench">weighted geometric mean</th>
            ${gmCells}
          </tr>`;

  const skippedHtml = skipped.length > 0
    ? `<p><strong>Skipped engines:</strong> ${skipped
      .map((s) => `${escapeHtml(s.label)} (${escapeHtml(s.reason)}; ${escapeHtml(s.install)})`)
      .join(' | ')}</p>`
    : '<p><strong>Skipped engines:</strong> none</p>';

  return `<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta name="author" content="joji" />
    <title>TitanKV Compare Benchmark Report</title>
    <style>
      :root { --line: #ddd; }
      * { box-sizing: border-box; }
      body {
        margin: 0;
        padding: 10px;
        font-family: Helvetica, Arial, sans-serif;
        font-size: 14px;
      }
      h1 { margin: 0 0 6px; }
      h2 { margin: 16px 0 8px; }
      p { margin: 0 0 8px; }
      table {
        border-collapse: collapse;
        table-layout: fixed;
        width: max-content;
        min-width: 860px;
        font-size: 11px;
      }
      th, td {
        border: 1px solid var(--line);
        padding: 4px;
        text-align: center;
        white-space: nowrap;
      }
      th.bench {
        position: sticky;
        left: 0;
        width: 260px;
        text-align: left;
        background: #fff;
        z-index: 2;
      }
      td.fast { background: #7ac67a; }
      td.mid { background: #e8db7d; }
      td.slow { background: #ef7f7f; }
      small { display: block; font-size: 10px; }
      .table-wrap { overflow: auto; border: 1px solid var(--line); width: 100%; }
      .summary-table { min-width: 1500px; }
      .summary-table th:first-child,
      .summary-table td:first-child,
      .summary-table th:nth-child(2),
      .summary-table td:nth-child(2) {
        position: sticky;
        background: #fff;
        z-index: 2;
      }
      .summary-table th:first-child,
      .summary-table td:first-child { left: 0; width: 50px; text-align: center; }
      .summary-table th:nth-child(2),
      .summary-table td:nth-child(2) { left: 50px; width: 180px; text-align: left; }
      .meta {
        width: 100%;
        min-width: 680px;
      }
      .meta th {
        text-align: left;
        width: 140px;
        background: #fafafa;
      }
      .meta td {
        text-align: left;
      }
      .mono { font-family: Consolas, Menlo, monospace; }
      .note {
        border: 1px solid var(--line);
        background: #fafafa;
        padding: 8px;
      }
    </style>
  </head>
  <body>
    <h1>TitanKV Compare Benchmark Report</h1>
    <p>
      TitanKV and similar embedded/local engines benchmarked on identical operations.
      Colors: green = faster, yellow = medium, red = slower (based on slowdown vs fastest active engine).
    </p>
    <div class="note">
      <p><strong>Interpretation:</strong> lower milliseconds and lower geometric mean are better.</p>
      <p><strong>Reliability:</strong> each engine is measured on fresh database directories and aggregated as median.</p>
    </div>
    <p class="mono">
      Run: ${escapeHtml(startedAt.toISOString())} | iterations: ${fmtNum(ITERATIONS)} | warmup: ${fmtNum(WARMUP)} | rounds: ${fmtNum(ROUNDS)}
    </p>
    ${stageHtml}
    ${skippedHtml}

    <h2>Environment</h2>
    <div class="table-wrap">
      ${systemHtml}
    </div>

    <h2>Engine Summary</h2>
    <div class="table-wrap">
      <table class="summary-table">
        <thead>
          <tr>
            <th>Rank</th>
            <th>Engine</th>
            <th>Geo Mean</th>
            <th>Put ms</th>
            <th>Get ms</th>
            <th>Miss ms</th>
            <th>Mixed ms</th>
            <th>Hot-key ms</th>
            <th>Large Put ms</th>
            <th>Large Get ms</th>
            <th>Stability CV</th>
          </tr>
        </thead>
        <tbody>
          ${summaryHtml}
        </tbody>
      </table>
    </div>

    <h2>Throughput (ops/s)</h2>
    <div class="table-wrap">
      <table>
        <thead>
          <tr>
            <th class="bench">Name / Throughput for...</th>
            ${headers}
          </tr>
        </thead>
        <tbody>
          ${throughputHtml}
        </tbody>
      </table>
    </div>

    <h2>Duration in milliseconds (Slowdown = Duration / Fastest)</h2>
    <div class="table-wrap">
      <table>
        <thead>
          <tr>
            <th class="bench">Name / Duration for...</th>
            ${headers}
          </tr>
        </thead>
        <tbody>
          ${bodyRows}
        </tbody>
      </table>
    </div>
  </body>
</html>`;
}

async function main() {
  const startedAt = new Date();
  const systemInfo = getSystemInfo();
  ensureDir(DATA_ROOT);
  ensureDir(REPORTS_DIR);

  console.log('TitanKV compare benchmark starting...');
  console.log(`iterations=${ITERATIONS}, warmup=${WARMUP}, rounds=${ROUNDS}, burnin=${BURNIN_ROUNDS}`);
  console.log(`largeValue=${LARGE_VALUE_BYTES} bytes x ${LARGE_VALUE_COUNT}`);
  console.log(`order=${ENGINE_DEFS.map((e) => e.label).join(' -> ')}`);
  console.log(`cpu=${systemInfo.cpuModel}`);
  if (MAX_CV > 0) {
    console.log(`stability gate enabled: COMPARE_MAX_CV=${MAX_CV}%`);
  }
  if (ITERATIONS < 2000) {
    console.log('warning: low iteration count can cause visible run-to-run variance; use >=2000 for more stable comparisons.');
  }

  const results = [];
  const unstable = [];
  for (let i = 0; i < ENGINE_DEFS.length; i++) {
    const def = ENGINE_DEFS[i];
    const stageIndex = i + 1;

    console.log(`[stage ${stageIndex}/${ENGINE_DEFS.length}] ${def.label} ...`);
    const result = await runEngineBenchmark(def);
    results.push(result);

    if (result.skipped) {
      console.log(`  skipped: ${result.reason}`);
    } else {
      const engineMaxCv = Math.max(...Object.values(result.metricsCv));
      console.log(
        `  put=${result.metricsMs.put.toFixed(1)}ms, get=${result.metricsMs.get.toFixed(1)}ms, miss=${result.metricsMs.miss.toFixed(1)}ms, mixed=${result.metricsMs.mixed.toFixed(1)}ms (median/${result.rounds} rounds, burnin=${result.burninRounds}, cv=${result.metricsCv.mixed.toFixed(1)}%)`
      );
      if (MAX_CV > 0 && engineMaxCv > MAX_CV) {
        unstable.push({ label: result.label, maxCv: engineMaxCv });
        console.log(`  warning: ${result.label} instability maxCV=${engineMaxCv.toFixed(1)}% > ${MAX_CV}%`);
      }
    }

    const html = renderHtml(results, startedAt, systemInfo, {
      index: stageIndex,
      total: ENGINE_DEFS.length,
      label: buildStageLabel(results),
    });

    const stagePath = path.join(REPORTS_DIR, stageFileName(stageIndex, def.key));
    fs.writeFileSync(stagePath, html, 'utf8');
    fs.writeFileSync(LOCAL_REPORT_PATH, html, 'utf8');

    console.log(`  stage report written: ${stagePath}`);
  }

  console.log(`latest report written: ${LOCAL_REPORT_PATH}`);
  console.log(`all staged reports: ${REPORTS_DIR}`);

  if (unstable.length > 0) {
    console.error(`stability gate failed (COMPARE_MAX_CV=${MAX_CV}%): ${unstable.map((u) => `${u.label}=${u.maxCv.toFixed(1)}%`).join(', ')}`);
    process.exitCode = 1;
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
