# Engine Notes

This folder is reserved for per-engine adapter modules if the compare runner is later split.

Current implementation keeps adapters in `benchmark/compare/run.js` for fast setup.

## Optional engine packages

- `better-sqlite3`
- `lmdb`
- `level`
- `rocksdb`

If a package is not installed, that engine is skipped automatically.
