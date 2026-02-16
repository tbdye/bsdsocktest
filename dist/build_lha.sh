#!/bin/sh
# Build an LHA distribution archive for Aminet.
# Run from the project root: sh dist/build_lha.sh
set -e

SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Extract version from tap.h
VERSION=$(grep '#define BSDSOCKTEST_VERSION' src/tap.h | sed 's/.*"\(.*\)".*/\1/')
if [ -z "$VERSION" ]; then
    echo "ERROR: could not extract version from src/tap.h" >&2
    exit 1
fi

echo "Building bsdsocktest v${VERSION} distribution archive..."

# Clean build to ensure a fresh binary
make clean
make

# Verify the binary exists
if [ ! -f bsdsocktest ]; then
    echo "ERROR: binary 'bsdsocktest' not found after build" >&2
    exit 1
fi

# Create staging directory
STAGING=$(mktemp -d)
trap 'rm -rf "$STAGING"' EXIT

mkdir -p "$STAGING/bsdsocktest/host"

cp bsdsocktest           "$STAGING/bsdsocktest/"
cp bsdsocktest.readme    "$STAGING/bsdsocktest/"
cp dist/bsdsocktest.info "$STAGING/bsdsocktest/"
cp LICENSE               "$STAGING/bsdsocktest/"
cp host/bsdsocktest_helper.py "$STAGING/bsdsocktest/host/"

# Create the archive from the staging directory so paths start with bsdsocktest/
ARCHIVE="bsdsocktest-${VERSION}.lha"
(cd "$STAGING" && jlha c "$PROJECT_ROOT/$ARCHIVE" bsdsocktest)

SIZE=$(ls -l "$ARCHIVE" | awk '{print $5}')
echo "Created ${ARCHIVE} (${SIZE} bytes)"
