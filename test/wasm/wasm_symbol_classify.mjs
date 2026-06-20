// Classify an Itanium-mangled C++ symbol by its OWN leading namespace, and
// decide whether that namespace is host-provided by the duckdb-wasm runtime.
//
// This is the core of duck_hunt's WASM guard. The hard requirement: the broken
// failure mode (a dependency symbol like YAML::Load / vtable for YAML::Exception
// imported but undefined) MUST be classified as foreign, while the symbols a
// clean duck_hunt build legitimately imports from the host (duckdb::, the
// bundled duckdb_yyjson::, and emscripten's libc++ / libc++abi / libstdc++) MUST
// be classified as host-provided.
//
// Why anchored classification, not substring matching: nearly every C++ library
// function takes or returns std:: types, so a naive `/St\d/` or `/_ZTV/` test
// matches deep inside a dependency symbol's parameter list and would wrongly
// allowlist exactly the symbols we must catch (e.g.
// _ZN4YAML4LoadERKNSt7__cxx1112basic_stringIc...EE contains "NSt7"). We instead
// extract the entity's own leading namespace token and allow only if THAT token
// is host-provided.

const ALLOWED_NS = new Set(["duckdb", "duckdb_yyjson", "__cxxabiv1", "__gnu_cxx"]);

// Returns the leading-namespace token of a mangled symbol, or a synthetic tag
// ("operator", "std", "std-sub"), or null if it is not a C++ symbol / can't be
// classified. null is treated as foreign by callers (conservative).
export function classify(name) {
  if (!/^_Z/.test(name)) return null; // not Itanium-mangled (C symbol, etc.)
  let s = name.slice(2);

  // global operator new / delete: _Znwm, _Znam, _ZdlPv, _ZdaPvm, ...
  if (/^n[wa]/.test(s) || /^d[la]/.test(s)) return "operator";

  // special-name prefixes that wrap a type/name:
  //   T[V|T|I|S|C|c] = vtable / VTT / typeinfo / typeinfo-name / construction-vtable
  //   GV             = guard variable
  //   T[h|v]<offset> = virtual / non-virtual thunk (strip the offset, then the target)
  if (/^T[VTISCc]/.test(s)) s = s.slice(2);
  else if (/^GV/.test(s)) s = s.slice(2);
  else if (/^T[hv]/.test(s)) { s = s.slice(2); s = s.replace(/^n?\d+_/, ""); }

  // nested-name: N [CV-qualifiers] <component>... E  -> strip N and any CV/ref
  if (s[0] === "N") { s = s.slice(1); s = s.replace(/^[rVK]+/, ""); }

  // std:: and its substitution abbreviations (Itanium <substitution>)
  if (/^St[\dE_]/.test(s) || s === "St") return "std";        // std::
  if (/^S[absiod]([A-Za-z0-9_]|$)/.test(s)) return "std";     // Sa/Ss/Si/So/Sd/Sb (allocator/string/...)
  if (/^S[0-9A-Z]?_/.test(s)) return "std-sub";               // generic back-reference substitution

  // source-name: <length><identifier>
  const m = s.match(/^(\d+)/);
  if (m) return s.slice(m[1].length, m[1].length + Number(m[1]));

  return null;
}

// extraAllow: array of RegExp escape-hatch patterns matched against the FULL
// symbol name (for the rare case you must allow a specific symbol by hand).
export function hostAllowed(name, extraAllow = []) {
  const c = classify(name);
  if (c === "operator" || c === "std" || c === "std-sub") return true;
  // Any "duckdb"-prefixed namespace is host-provided: the duckdb-wasm runtime
  // exports its own symbols AND its vendored third_party, which all live under a
  // duckdb_* namespace (duckdb_yyjson, duckdb_zstd, duckdb_re2, duckdb_miniz, ...).
  // Verified against the host module's export table — e.g. scalarfs imports
  // duckdb_zstd::ZSTD_* and the host exports every one of them. A hardcoded list
  // (duckdb_yyjson only) would FALSE-POSITIVE any other bundled lib (zstd, re2).
  // Real foreign deps never use a duckdb_ prefix (yaml-cpp=YAML, libxml2=xml,
  // cmark=cmark), so the prefix match cannot mask a genuine missing dependency.
  if (c !== null && (c === "duckdb" || c.startsWith("duckdb_") || ALLOWED_NS.has(c))) return true;
  return extraAllow.some((re) => re.test(name));
}
