#!/usr/bin/env bash
# Run the WASM unresolved-symbol check against built .wasm artifacts.
#
# This is the fast, deterministic gate for the LINKED_LIBS class of bug
# (see check_wasm_imports.mjs and README.md for the full explanation). It does
# NOT need a duckdb-wasm runtime or a matching duckdb version -- it statically
# inspects the side module's imports. Run it after `make wasm_mvp` / `wasm_eh`,
# or in CI against the downloaded build artifacts.
#
# duck_hunt links no vcpkg dependencies, so the check runs in ALLOWLIST mode:
# it fails if any not-self-resolved C++ symbol appears outside the known
# host-provided set (duckdb / duckdb_yyjson / libc++). The expected result is
# "0 foreign" -- a failure means a dependency was added without wiring
# LINKED_LIBS in extension_config.cmake (see README.md).
#
# Usage: test/wasm/run_wasm_checks.sh [build-dir ...]
#   defaults to build/wasm_mvp build/wasm_eh build/wasm_threads if present.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$here/../.." && pwd)"

# Verify the classifier itself before trusting its verdict (guards the FAIL path
# that real .wasm builds never exercise -- see selftest.mjs).
node "$here/selftest.mjs" || { echo "classifier self-test failed; aborting" >&2; exit 1; }

dirs=("$@")
if [ ${#dirs[@]} -eq 0 ]; then
  for d in build/wasm_mvp build/wasm_eh build/wasm_threads; do
    [ -d "$repo_root/$d" ] && dirs+=("$repo_root/$d")
  done
fi

if [ ${#dirs[@]} -eq 0 ]; then
  echo "no wasm build dirs found; build first (e.g. make wasm_mvp)" >&2
  exit 2
fi

found_any=0
rc=0
for d in "${dirs[@]}"; do
  while IFS= read -r -d '' wasm; do
    found_any=1
    echo "==> checking $wasm"
    node "$here/check_wasm_imports.mjs" "$wasm" --verbose || rc=1
  done < <(find "$d" -name '*.duckdb_extension.wasm' -print0 2>/dev/null)
done

if [ "$found_any" -eq 0 ]; then
  echo "no *.duckdb_extension.wasm found under: ${dirs[*]}" >&2
  exit 2
fi
exit $rc
