# This file is included by DuckDB's build system. It specifies which extension to load

# Extension from this repo
duckdb_extension_load(duck_hunt
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
    # NOTE: no LINKED_LIBS here on purpose. The WASM side module is linked by a
    # separate `emcc -sSIDE_MODULE=2 ... ${LINKED_LIBS}` step that ignores
    # target_link_libraries() and pulls in ONLY the libs named here (this is the
    # bug duckdb_yaml #40 / duckdb_webbed #96 fixed by naming yaml-cpp/libxml2).
    # duck_hunt statically links nothing (vcpkg.json is empty): it uses duckdb's
    # bundled yyjson (host-provided) and calls webbed/zipfs via SQL, never by
    # linking their symbols. If you add a vcpkg dependency, you MUST add it here
    # or the .wasm will ship with unresolved imports. test/wasm/ guards this.
)

# Any extra extensions that should be built
duckdb_extension_load(json)
