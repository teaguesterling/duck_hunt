# WASM build tests

These tests guard the DuckDB-WASM build of duck_hunt against the failure mode
described in Rusty Conover's
[Testing DuckDB WebAssembly Extensions](https://rusty.today/blog/testing-duckdb-wasm-extensions/):
a `.duckdb_extension.wasm` that **builds and publishes green but fails to load
(or throws at first call)** because a dependency's symbols are left unresolved
in the side module. The sibling extensions
[duckdb_yaml #40](https://github.com/teaguesterling/duckdb_yaml/issues/40) and
[duckdb_webbed #96](https://github.com/teaguesterling/duckdb_webbed/issues/96)
hit exactly this with yaml-cpp / libxml2.

## Why duck_hunt is different (and what we actually found)

The yaml/webbed bug is specifically a **`LINKED_LIBS`** bug. The loadable
`*.duckdb_extension.wasm` is **not** produced by the normal CMake link: DuckDB's
`extension/extension_build_tools.cmake` runs a separate
`emcc -sSIDE_MODULE=2 ... ${LINKED_LIBS}` step that links the extension archive
against **only** the libraries named in `LINKED_LIBS` in
`extension_config.cmake`. `target_link_libraries(...)` in `CMakeLists.txt` is
honored for native builds but **ignored** by that emcc step. If a statically
linked vcpkg dep (yaml-cpp, libxml2) is not in `LINKED_LIBS`, its symbols become
unresolved imports in the `.wasm`.

**duck_hunt statically links nothing.** `vcpkg.json` has zero dependencies. It
parses JSON with duckdb's bundled `yyjson` (one call, `yyjson_read_opts`), and
optionally calls into the `webbed` (XML) and `zipfs` (ZIP) extensions — but only
by issuing SQL (`SELECT xml_to_json(...)`), never by linking their symbols, and
with graceful fallback when they aren't loaded. So there is **no analog of the
yaml-cpp / libxml2 problem**, and nothing to put in `LINKED_LIBS`.

Empirically, on the **published** artifacts (downloaded from
`community-extensions.duckdb.org`, both `v1.5.3` and `v1.4.4`, `wasm_eh` and
`wasm_mvp`):

```
imports: 3230 | not-self-resolved: 232  (98 duckdb:: + 1 duckdb_yyjson:: + ~133 libc++/std/runtime)
foreign (outside host allowlist): 0
```

Every not-self-resolved import is host-provided by the duckdb-wasm runtime
(the duckdb C++ runtime, the bundled `json`/yyjson, emscripten's libc++). The
single `duckdb_yyjson::yyjson_read_opts` import resolves against the host's
bundled `json` exactly as it does against the duckdb process in a native build —
the same mechanism, which already works. **No WASM bug was found; this harness
is a forward-looking regression guard, not a fix.**

## The guard: allowlist, not forbid

Because there are no static deps to *forbid*, the meaningful check is the
inverse — an **allowlist**. `check_wasm_imports.mjs` parses the built `.wasm`
(validate-only, **no instantiation**) and fails if any not-self-resolved
**C++-mangled** symbol appears outside the known host-provided set
(`duckdb::`, `duckdb_yyjson::`, `std::`/`__gnu_cxx`/`__cxxabiv1`, typeinfo/vtable,
operator new/delete). That flags the day someone adds a vcpkg dependency
without wiring `LINKED_LIBS`: its symbols would appear as a new foreign
namespace and trip the check.

Subtleties (same as the sibling harnesses):

- **"Is it imported" is the wrong question.** emscripten side modules use
  interposition linking, so a module legitimately *imports* many symbols it also
  *defines/exports*, and legitimately imports host-provided symbols. The real
  bug is a symbol that is imported, **not** self-exported, and **not**
  host-provided. Imports are resolved per bucket against the right export kind:
  `env`/`GOT.func` → exported functions, `GOT.mem` → exported data globals (so
  unresolved **vtables** `_ZTV*` / typeinfo `_ZTI*` are caught too, not just
  functions).
- **Static, not instantiation-based.** It catches the bug whether the missing
  symbol would fail at *load* time or only at first *call*, and needs **no**
  matching duckdb-wasm engine, so a red result is unambiguously a missing
  dependency and not an ABI/version mismatch.
- **Anchored classification, not substring matching.** Namespace classification
  (`wasm_symbol_classify.mjs`) keys on each symbol's *own* leading entity
  namespace. A naive `/St\d/` or `/_ZTV/` substring test would match deep inside
  a dependency symbol's `std::` parameter list (e.g.
  `_ZN4YAML4LoadERKNSt7__cxx11...`) and wrongly allowlist the very symbols we
  must catch. `selftest.mjs` guards this failure path with hardcoded
  yaml-cpp/libxml2 fixtures — it needs no `.wasm` and runs first in
  `run_wasm_checks.sh`.

### Files

- `wasm_symbol_classify.mjs` — anchored namespace classifier + host allowlist.
- `check_wasm_imports.mjs` — parses a `.wasm`, reports foreign symbols (the gate).
- `selftest.mjs` — artifact-free assertions that the classifier flags real
  dependency symbols and allows host symbols (guards the FAIL path).
- `run_wasm_checks.sh` — runs the self-test, then checks every `.wasm` in the
  given (or default `build/wasm_*`) dirs.

### Limitation

The allowlist classifies **C++-mangled** symbols. A pure-**C** dependency added
without `LINKED_LIBS` would import unmangled names (e.g. `inflate`) that the
allowlist can't distinguish from emscripten libc. If you ever link such a
dependency, also pass `--forbid '<symbol-regex>'` (the script still supports the
yaml/webbed forbid mode), and add it to `LINKED_LIBS`.

## Running

```bash
make wasm_mvp                       # or wasm_eh / wasm_threads
test/wasm/run_wasm_checks.sh        # scans build/wasm_* for *.duckdb_extension.wasm
```

Exit non-zero = foreign (non-host) dependency symbols found.

You can also point it at a directory of already-built artifacts (this is what CI
does with the downloaded build artifacts):

```bash
test/wasm/run_wasm_checks.sh path/to/wasm-artifacts
```

Or check a single artifact directly (e.g. one downloaded from the community
registry — the 256-byte signature footer is handled automatically):

```bash
node test/wasm/check_wasm_imports.mjs duck_hunt.duckdb_extension.wasm --verbose
```

## Layer 2 — live load test in duckdb-wasm (`load_test.cjs`)

The static check proves symbols *resolve*; it does **not** prove the module
instantiates and *runs*. `load_test.cjs` (adopted from
[duckdb_webbed](https://github.com/teaguesterling/duckdb_webbed)) closes that
gap: it instantiates a duckdb-wasm engine in Node, serves the built extension
repository over HTTP, `INSTALL`s + `LOAD`s duck_hunt, and runs a query. It uses
the **async** API (the blocking API deadlocks on `LOAD`) and a watchdog timeout
so a mismatched engine can't hang the run. Needs the npm deps in `package.json`.

```bash
cd test/wasm && npm install
mkdir -p repo/v1.5.3/wasm_eh
cp ../../build/wasm_eh/repository/v1.5.3/wasm_eh/duck_hunt.duckdb_extension.wasm repo/v1.5.3/wasm_eh/
# (or drop in a published artifact from community-extensions.duckdb.org)
node load_test.cjs --repo repo --name duck_hunt --platform eh \
    --query "SELECT duck_hunt_detect_format('{\"tests\":[{\"nodeid\":\"a::b\",\"outcome\":\"failed\"}]}') AS fmt" \
    --expect '"fmt":"pytest_json"'
```

That query is deliberately chosen to exercise the **yyjson call path** — the one
host-provided symbol duck_hunt depends on — via the pytest_json parser's
`can_parse`. A clean run is the end-to-end proof that the host actually provides
yyjson at call time, not just that the import resolves.

**Engine matching (important).** A clean `LOAD` requires an engine matching
**both** the extension's duckdb version (v1.5.3) **and** its emscripten ABI
(`extension-ci-tools@v1.5.3` pins emsdk 3.1.71). As of this writing no public
engine satisfies both, so the live test does not yet go green:

| engine | duckdb | result against the v1.5.3 artifact |
| --- | --- | --- |
| `@haybarn/haybarn-wasm` 1.5.3-rc15 | v1.5.4-dev135 (ahead) | engine boots + bare `SELECT` works + `INSTALL` works; `LOAD` **deadlocks** during side-module instantiation (version/ABI skew) — watchdog terminates it |
| `@duckdb/duckdb-wasm` 1.33 | v1.5.1 (behind) | `memory access out of bounds` (per webbed) |

In every case the failure is downstream of symbol resolution and `INSTALL` —
consistent with Layer 1's "0 foreign symbols" — and is a duckdb-version /
toolchain skew, **not** a duck_hunt defect. The live test will go green once a
public engine ships v1.5.3 built with the pinned emsdk. Until then it is wired
into CI as a **non-blocking** (informational) step; Layer 1 is the hard gate.

- Harness inspiration: https://github.com/Query-farm-haybarn/haybarn-extension-wasm-tester
- Writeup: https://rusty.today/blog/testing-duckdb-wasm-extensions/

## CI

`wasm-symbol-check` in `.github/workflows/MainDistributionPipeline.yml` downloads
the `wasm_*` artifacts built by the reusable `duckdb/extension-ci-tools`
distribution workflow (which builds and uploads the `.wasm` but never inspects
or loads it — which is why the yaml/webbed bug shipped) and runs:

- **Layer 1** (`run_wasm_checks.sh`) — the hard gate; needs only `node`.
- **Layer 2** (`load_test.cjs`, `continue-on-error: true`) — informational until
  a version-matched engine exists (see above).
