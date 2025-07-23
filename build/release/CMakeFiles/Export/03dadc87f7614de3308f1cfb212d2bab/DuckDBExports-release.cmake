#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "duckdb" for configuration "Release"
set_property(TARGET duckdb APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb.so"
  IMPORTED_SONAME_RELEASE "libduckdb.so"
  )

list(APPEND _cmake_import_check_targets duckdb )
list(APPEND _cmake_import_check_files_for_duckdb "${_IMPORT_PREFIX}/lib/libduckdb.so" )

# Import target "duckdb_static" for configuration "Release"
set_property(TARGET duckdb_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_static.a"
  )

list(APPEND _cmake_import_check_targets duckdb_static )
list(APPEND _cmake_import_check_files_for_duckdb_static "${_IMPORT_PREFIX}/lib/libduckdb_static.a" )

# Import target "duck_hunt_extension" for configuration "Release"
set_property(TARGET duck_hunt_extension APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duck_hunt_extension PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduck_hunt_extension.a"
  )

list(APPEND _cmake_import_check_targets duck_hunt_extension )
list(APPEND _cmake_import_check_files_for_duck_hunt_extension "${_IMPORT_PREFIX}/lib/libduck_hunt_extension.a" )

# Import target "core_functions_extension" for configuration "Release"
set_property(TARGET core_functions_extension APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(core_functions_extension PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libcore_functions_extension.a"
  )

list(APPEND _cmake_import_check_targets core_functions_extension )
list(APPEND _cmake_import_check_files_for_core_functions_extension "${_IMPORT_PREFIX}/lib/libcore_functions_extension.a" )

# Import target "parquet_extension" for configuration "Release"
set_property(TARGET parquet_extension APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(parquet_extension PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libparquet_extension.a"
  )

list(APPEND _cmake_import_check_targets parquet_extension )
list(APPEND _cmake_import_check_files_for_parquet_extension "${_IMPORT_PREFIX}/lib/libparquet_extension.a" )

# Import target "jemalloc_extension" for configuration "Release"
set_property(TARGET jemalloc_extension APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(jemalloc_extension PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libjemalloc_extension.a"
  )

list(APPEND _cmake_import_check_targets jemalloc_extension )
list(APPEND _cmake_import_check_files_for_jemalloc_extension "${_IMPORT_PREFIX}/lib/libjemalloc_extension.a" )

# Import target "duckdb_fmt" for configuration "Release"
set_property(TARGET duckdb_fmt APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_fmt PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_fmt.a"
  )

list(APPEND _cmake_import_check_targets duckdb_fmt )
list(APPEND _cmake_import_check_files_for_duckdb_fmt "${_IMPORT_PREFIX}/lib/libduckdb_fmt.a" )

# Import target "duckdb_duckdb_pg_query" for configuration "Release"
set_property(TARGET duckdb_duckdb_pg_query APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_duckdb_pg_query PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_pg_query.a"
  )

list(APPEND _cmake_import_check_targets duckdb_duckdb_pg_query )
list(APPEND _cmake_import_check_files_for_duckdb_duckdb_pg_query "${_IMPORT_PREFIX}/lib/libduckdb_pg_query.a" )

# Import target "duckdb_re2" for configuration "Release"
set_property(TARGET duckdb_re2 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_re2 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_re2.a"
  )

list(APPEND _cmake_import_check_targets duckdb_re2 )
list(APPEND _cmake_import_check_files_for_duckdb_re2 "${_IMPORT_PREFIX}/lib/libduckdb_re2.a" )

# Import target "duckdb_duckdb_miniz" for configuration "Release"
set_property(TARGET duckdb_duckdb_miniz APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_duckdb_miniz PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_miniz.a"
  )

list(APPEND _cmake_import_check_targets duckdb_duckdb_miniz )
list(APPEND _cmake_import_check_files_for_duckdb_duckdb_miniz "${_IMPORT_PREFIX}/lib/libduckdb_miniz.a" )

# Import target "duckdb_utf8proc" for configuration "Release"
set_property(TARGET duckdb_utf8proc APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_utf8proc PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_utf8proc.a"
  )

list(APPEND _cmake_import_check_targets duckdb_utf8proc )
list(APPEND _cmake_import_check_files_for_duckdb_utf8proc "${_IMPORT_PREFIX}/lib/libduckdb_utf8proc.a" )

# Import target "duckdb_hyperloglog" for configuration "Release"
set_property(TARGET duckdb_hyperloglog APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_hyperloglog PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_hyperloglog.a"
  )

list(APPEND _cmake_import_check_targets duckdb_hyperloglog )
list(APPEND _cmake_import_check_files_for_duckdb_hyperloglog "${_IMPORT_PREFIX}/lib/libduckdb_hyperloglog.a" )

# Import target "duckdb_skiplistlib" for configuration "Release"
set_property(TARGET duckdb_skiplistlib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_skiplistlib PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_skiplistlib.a"
  )

list(APPEND _cmake_import_check_targets duckdb_skiplistlib )
list(APPEND _cmake_import_check_files_for_duckdb_skiplistlib "${_IMPORT_PREFIX}/lib/libduckdb_skiplistlib.a" )

# Import target "duckdb_fastpforlib" for configuration "Release"
set_property(TARGET duckdb_fastpforlib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_fastpforlib PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_fastpforlib.a"
  )

list(APPEND _cmake_import_check_targets duckdb_fastpforlib )
list(APPEND _cmake_import_check_files_for_duckdb_fastpforlib "${_IMPORT_PREFIX}/lib/libduckdb_fastpforlib.a" )

# Import target "duckdb_mbedtls" for configuration "Release"
set_property(TARGET duckdb_mbedtls APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_mbedtls PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_mbedtls.a"
  )

list(APPEND _cmake_import_check_targets duckdb_mbedtls )
list(APPEND _cmake_import_check_files_for_duckdb_mbedtls "${_IMPORT_PREFIX}/lib/libduckdb_mbedtls.a" )

# Import target "duckdb_fsst" for configuration "Release"
set_property(TARGET duckdb_fsst APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_fsst PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_fsst.a"
  )

list(APPEND _cmake_import_check_targets duckdb_fsst )
list(APPEND _cmake_import_check_files_for_duckdb_fsst "${_IMPORT_PREFIX}/lib/libduckdb_fsst.a" )

# Import target "duckdb_duckdb_yyjson" for configuration "Release"
set_property(TARGET duckdb_duckdb_yyjson APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_duckdb_yyjson PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_yyjson.a"
  )

list(APPEND _cmake_import_check_targets duckdb_duckdb_yyjson )
list(APPEND _cmake_import_check_files_for_duckdb_duckdb_yyjson "${_IMPORT_PREFIX}/lib/libduckdb_yyjson.a" )

# Import target "duckdb_duckdb_zstd" for configuration "Release"
set_property(TARGET duckdb_duckdb_zstd APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(duckdb_duckdb_zstd PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libduckdb_zstd.a"
  )

list(APPEND _cmake_import_check_targets duckdb_duckdb_zstd )
list(APPEND _cmake_import_check_files_for_duckdb_duckdb_zstd "${_IMPORT_PREFIX}/lib/libduckdb_zstd.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
