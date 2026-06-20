#!/usr/bin/env node
// Self-test for the WASM symbol classifier (wasm_symbol_classify.mjs).
//
// The guard's value rests entirely on classify()/hostAllowed() correctly
// telling a host-provided symbol from a foreign dependency symbol. The
// PASS path is exercised by every CI build against the real .wasm; this test
// guards the FAIL path -- that the symbols a LINKED_LIBS regression actually
// produces are flagged foreign, and that the symbols a clean build legitimately
// imports are allowed. It needs no .wasm artifact, so it runs anywhere with node.
//
// Exit: 0 = all assertions pass, 1 = a regression in the classifier.

import { classify, hostAllowed } from "./wasm_symbol_classify.mjs";

// MUST be flagged foreign (hostAllowed === false). These are the real symbols a
// yaml-cpp/libxml2-style "forgot LINKED_LIBS" regression leaves unresolved
// (mangled forms taken from duckdb_yaml #40). The std:: in the parameter list of
// the first one is the exact trap that defeats substring matching.
const MUST_BE_FOREIGN = [
  "_ZN4YAML4LoadERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE", // YAML::Load(std::string const&)
  "_ZTVN4YAML9ExceptionE",                                               // vtable for YAML::Exception
  "_ZTIN4YAML9ExceptionE",                                               // typeinfo for YAML::Exception
  "_ZN4YAML7Emitter4emitERKNS_4NodeE",                                   // YAML::Emitter::emit(YAML::Node const&)
  "_ZN4YAML6detail9node_data8convertEv",                                 // YAML::detail::node_data::convert()
  "_ZN6xmlpp8DocumentC1Ev",                                              // libxml++ Document ctor (webbed-style)
  "inflate",                                                             // pure-C dep (zlib) -- not classifiable -> foreign
];

// MUST be allowed (hostAllowed === true). These are host-provided: duckdb
// runtime, the bundled duckdb_yyjson, and emscripten libc++/libc++abi/libstdc++.
const MUST_BE_ALLOWED = [
  "_ZN6duckdb9ExceptionC2ENS_13ExceptionTypeERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE", // duckdb::Exception ctor
  "_ZN6duckdb10StringUtil7ReplaceENS_6stringERKS1_S3_",                  // duckdb::StringUtil::Replace
  "_ZN13duckdb_yyjson16yyjson_read_optsEPcmjPKNS_10yyjson_alcEPNS_15yyjson_read_errE", // host yyjson
  "_ZN11duckdb_zstd18ZSTD_createDStreamEv",                              // host duckdb_zstd (e.g. scalarfs) — bundled third_party
  "_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE6appendEPKcm",   // std::string::append
  "_ZSt19__throw_logic_errorPKc",                                        // std::__throw_logic_error
  "_ZTVSt12length_error",                                                // vtable for std::length_error
  "_ZN9__gnu_cxx27__verbose_terminate_handlerEv",                        // __gnu_cxx::...
  "_ZTIN10__cxxabiv117__class_type_infoE",                               // __cxxabiv1::...
  "_Znwm",                                                               // operator new
  "_ZdlPv",                                                              // operator delete
];

let failed = 0;
for (const s of MUST_BE_FOREIGN) {
  if (hostAllowed(s)) { failed++; console.error(`FAIL (should be foreign, got allowed): classify=${classify(s)}  ${s}`); }
}
for (const s of MUST_BE_ALLOWED) {
  if (!hostAllowed(s)) { failed++; console.error(`FAIL (should be allowed, got foreign): classify=${classify(s)}  ${s}`); }
}

// The --allow escape hatch must be able to allow an otherwise-foreign symbol.
if (!hostAllowed("_ZN4YAML4LoadEv", [/4YAML/])) { failed++; console.error("FAIL: --allow escape hatch did not allow a matched symbol"); }

if (failed) {
  console.error(`\nclassifier self-test: ${failed} assertion(s) FAILED`);
  process.exit(1);
}
console.error(`classifier self-test: OK (${MUST_BE_FOREIGN.length} foreign + ${MUST_BE_ALLOWED.length} allowed + escape-hatch all correct)`);
process.exit(0);
