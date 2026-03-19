---
trigger: always_on
---

Coding Style: Adopt a minimalist, "hacker/OOP js pro dev" aesthetic: do NOT use semicolons (rely on ASI), use CommonJS (require), and use short but descriptive variable names.

Architecture: Avoid deep class inheritance (class A extends B); instead, use Functional Mixins (e.g., class Store extends Monitorable(Configurable(Base))).

Control Flow (NASA Rule 1): Do NOT use recursion or setInterval; use named IIFEs with setTimeout for async loops (e.g., (function work() { ... setTimeout(work, 100) }())) to ensure stack safety.

Loop Safety (NASA Rule 2): Every loop MUST have a fixed upper bound or a safety counter; throw a critical error if the limit is exceeded.

Memory Safety (NASA Rule 3): Do not allocate new memory (like new Buffer or large objects) inside loops or hot paths; pre-allocate resources at initialization.

Function Size (NASA Rule 4): Limit all functions to a maximum of 60 lines; refactor anything longer into smaller, logical units.

Defensive Coding (NASA Rule 5): Include at least two assertions/checks at the start of every function to validate arguments (e.g., if (!arg) throw new Error()).

Logging: STRICTLY FORBIDDEN to use console.log; always use the debug module (e.g., const debug = require('debug')('app:module')).

Git Behavior: Write professional, human-like commit messages (e.g., "core: fix race condition in worker #42") and avoiding generic AI messages like "update file".

Testing: Treat code without tests as non-existent; every new feature must have a corresponding unit test file.