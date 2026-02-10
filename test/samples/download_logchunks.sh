#!/bin/bash
# Download LogChunks dataset for Travis CI build log testing
# Source: https://zenodo.org/records/3632351
# License: CC-BY-4.0

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d "LogChunks" ]; then
    echo "LogChunks already exists. Remove it first to re-download."
    exit 0
fi

echo "Downloading LogChunks dataset (24 MB)..."
curl -L -o logchunks.zip "https://zenodo.org/records/3632351/files/LogChunks.zip?download=1"

echo "Extracting..."
unzip -q logchunks.zip
rm logchunks.zip

# Clean up macOS artifacts if present
rm -rf __MACOSX 2>/dev/null || true

echo "Done! LogChunks dataset is available at: $SCRIPT_DIR/LogChunks"
echo ""
echo "Structure:"
echo "  LogChunks/logs/<language>/<repo>/failed/<build_id>.log - Raw Travis CI logs"
echo "  LogChunks/build-failure-reason/<language>/<repo>.xml - Labeled failure chunks"
echo ""
echo "Languages: $(ls LogChunks/logs | wc -l) (C, C++, Python, Java, JavaScript, Ruby, Go, Rust, etc.)"
echo "Total logs: $(find LogChunks/logs -name '*.log' | wc -l)"
