#!/usr/bin/env node
// WASM side-module unresolved-dependency-symbol check (duck_hunt edition).
//
// Why this exists: the loadable `.duckdb_extension.wasm` is produced by a
// separate `emcc -sSIDE_MODULE=2 ... ${LINKED_LIBS}` step (see duckdb's
// extension/extension_build_tools.cmake). That link pulls in ONLY the libraries
// named in `LINKED_LIBS` in extension_config.cmake -- `target_link_libraries`
// in CMakeLists.txt is ignored for it. An extension that statically links a
// vcpkg dep (yaml-cpp, libxml2, ...) and forgets to name it in LINKED_LIBS ships
// a side module whose dependency symbols are imported but defined nowhere; the
// duckdb-wasm host does not provide them, so the module fails to load
// ("could not load dynamic lib") or throws at first call. CI builds the .wasm
// but never inspects it, so this ships green. See Rusty Conover's writeup
// (https://rusty.today/blog/testing-duckdb-wasm-extensions/) and README.md.
//
// duck_hunt statically links NOTHING (vcpkg.json has zero deps). Its only
// foreign symbol is one yyjson call (duckdb_yyjson::yyjson_read_opts), which the
// duckdb-wasm host provides via the bundled `json` extension -- exactly as the
// native build resolves it against the host process. So there is nothing to
// "forbid": the meaningful guard is the inverse, an ALLOWLIST. We flag any
// not-self-resolved C++ symbol whose own leading namespace is NOT host-provided
// (duckdb / duckdb_yyjson / the C++ standard library). That catches the day
// someone adds a vcpkg dependency without wiring LINKED_LIBS -- its symbols show
// up under a new foreign namespace and fail this check. (A pure-C dependency
// added without LINKED_LIBS imports unmangled names the allowlist can't
// classify; use --forbid for those, and see the README limitation note.)
//
// Namespace classification is anchored to each symbol's OWN entity (see
// wasm_symbol_classify.mjs) -- NOT a substring match, which would wrongly
// allowlist a dependency symbol that merely takes a std:: argument. selftest.mjs
// guards that classifier's failure path.
//
// What "self-resolved" means -- and why "is it imported" is the WRONG question:
// emscripten side modules use position-independent / interposition linking, so a
// module legitimately IMPORTS many symbols it also DEFINES/EXPORTS, and also
// imports host-provided symbols. The genuine bug is a dependency symbol that is
// imported, NOT defined by this module, and not host-provided. Imports come in
// three relevant buckets, resolved against the right export kind:
//   - module "env",      kind function  -> direct call           -> exported fn
//   - module "GOT.func",  kind global    -> function address slot -> exported fn
//   - module "GOT.mem",   kind global    -> data address slot     -> exported global
// A GOT.mem entry is how data symbols (vtables _ZTV*, typeinfo _ZTI*) are
// referenced, so checking only function imports would miss an unresolved vtable.
//
// Static parse only (WebAssembly.Module) -- no instantiation, no duckdb-wasm
// runtime, no duckdb version match -- so a failure is unambiguously a missing
// dependency and not an ABI/version mismatch. Published artifacts carry a
// 256-byte duckdb signature footer; we strip it on a parse failure and retry.
//
// Usage:
//   node check_wasm_imports.mjs <path-to.wasm> [--forbid <regex> ...] [--allow <regex> ...] [--verbose]
//
//   (default)      ALLOWLIST mode: fail on any not-self-resolved C++ symbol
//                  whose own namespace is outside the host set.
//   --forbid <re>  additionally fail on any not-self-resolved import matching
//                  <re> (use for a known C dependency you've linked).
//   --allow <re>   extend the host allowlist with a full-name regex (escape hatch).
//
// Exit: 0 = clean, 1 = unresolved/forbidden dependency symbols found, 2 = usage/file error.

import { readFileSync } from "node:fs";
import { hostAllowed } from "./wasm_symbol_classify.mjs";

const argv = process.argv.slice(2);
const forbid = [];
const extraAllow = [];
let wasmPath = null;
let verbose = false;

for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === "--forbid") forbid.push(new RegExp(argv[++i]));
  else if (a === "--allow") extraAllow.push(new RegExp(argv[++i]));
  else if (a === "--verbose" || a === "-v") verbose = true;
  else if (!wasmPath) wasmPath = a;
  else { console.error(`unexpected argument: ${a}`); process.exit(2); }
}

if (!wasmPath) {
  console.error("usage: node check_wasm_imports.mjs <path-to.wasm> [--forbid <regex> ...] [--allow <regex> ...] [--verbose]");
  process.exit(2);
}

let bytes;
try { bytes = readFileSync(wasmPath); }
catch (e) { console.error(`cannot read ${wasmPath}: ${e.message}`); process.exit(2); }

let mod;
try {
  mod = new WebAssembly.Module(bytes);
} catch (e1) {
  // Published community artifacts append a 256-byte duckdb signature footer
  // that trips the (trailing-byte-strict) wasm parser. Strip and retry.
  try {
    mod = new WebAssembly.Module(bytes.subarray(0, bytes.length - 256));
    if (verbose) console.error("(stripped 256-byte signature footer to parse)");
  } catch (e2) {
    console.error(`not a valid wasm module: ${e1.message}`);
    process.exit(2);
  }
}

const imports = WebAssembly.Module.imports(mod);
const exports = WebAssembly.Module.exports(mod);
const expFn = new Set(exports.filter((e) => e.kind === "function").map((e) => e.name));
const expGl = new Set(exports.filter((e) => e.kind === "global").map((e) => e.name));

// Does this module itself satisfy the import? (interposition self-resolution)
function selfResolved(imp) {
  if (imp.kind === "function") return expFn.has(imp.name);     // env.<fn>
  if (imp.module === "GOT.func") return expFn.has(imp.name);   // function address
  if (imp.module === "GOT.mem") return expGl.has(imp.name) || expFn.has(imp.name); // data address
  if (imp.kind === "global") return expGl.has(imp.name);       // env global / data
  return false; // tables/memories etc. are host-provided runtime, never dep symbols
}

const isMangled = (name) => /^_Z/.test(name);
const notSelf = imports.filter((i) => !selfResolved(i));

// Default allowlist failures: not-self-resolved C++ symbols outside the host set.
const foreignCpp = notSelf.filter((i) => isMangled(i.name) && !hostAllowed(i.name, extraAllow));
// Explicit --forbid failures (optional, e.g. a linked C dependency).
const forbidden = forbid.length
  ? notSelf.filter((i) => forbid.some((re) => re.test(i.name)))
  : [];

if (verbose) {
  const byBucket = {};
  for (const u of notSelf) byBucket[u.module] = (byBucket[u.module] || 0) + 1;
  const mangled = notSelf.filter((i) => isMangled(i.name));
  console.error(`[check] ${wasmPath}`);
  console.error(`  imports: ${imports.length} | exports: ${expFn.size} fn, ${expGl.size} global`);
  console.error(`  not-self-resolved: ${notSelf.length} ${JSON.stringify(byBucket)} (host-provided expected)`);
  console.error(`  not-self-resolved C++-mangled: ${mangled.length} | foreign (outside host allowlist): ${foreignCpp.length}`);
}

const failures = [...new Set([...foreignCpp, ...forbidden].map((i) => `${i.module}.${i.name}`))];

if (failures.length > 0) {
  console.error(`FAIL: ${failures.length} unresolved foreign symbol(s) in ${wasmPath} (imported, not defined by the module, not a known host-provided symbol). A dependency is likely missing from LINKED_LIBS in extension_config.cmake:`);
  for (const f of failures.slice(0, 40)) console.error(`  ${f}`);
  if (failures.length > 40) console.error(`  ... and ${failures.length - 40} more`);
  process.exit(1);
}

console.error(`PASS: ${wasmPath} -- ${notSelf.length} not-self-resolved import(s), all host-provided (duckdb / yyjson / libc++); 0 foreign. ${imports.length} imports inspected.`);
process.exit(0);
