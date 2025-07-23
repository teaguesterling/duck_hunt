if [ "$1" == -v ] ; then 
    make 2>&1 | tee >(tee make.out 1>&2) | ./build/release/duckdb -s "INSTALL json; LOAD json; COPY ( SELECT * EXCLUDE (raw_output) FROM read_test_results('/dev/stdin', 'auto') ) TO '/dev/stdout' (FORMAT json);" | tee .build-logs/$(git rev-parse --short HEAD).json | jq . && ln -vsfn .build-logs/$(git rev-parse --short HEAD).json .build-logs/HEAD.json && echo DONE
    ALSO=">(tee make.out 1>&2)" ; 
else
    make 2>&1 | ./build/release/duckdb -s "INSTALL json; LOAD json; COPY ( SELECT * EXCLUDE (raw_output) FROM read_test_results('/dev/stdin', 'auto') ) TO '/dev/stdout' (FORMAT json);" | tee .build-logs/$(git rev-parse --short HEAD).json | jq . && ln -vsfn .build-logs/$(git rev-parse --short HEAD).json .build-logs/HEAD.json && echo DONE
fi
