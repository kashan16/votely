#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────
#  build.sh  —  Blockchain Voting: C++ build script
#
#  Usage:
#    ./build.sh           # compile & run native tests
#    ./build.sh wasm      # compile to WebAssembly via Emscripten
#    ./build.sh clean     # remove build artefacts
# ──────────────────────────────────────────────────────────────
set -euo pipefail

SOURCES="blockchain.cpp bindings.cpp"
WASM_OUT="../web/lib/wasm/blockchain"
NATIVE_OUT="./blockchain_test"

GRN='\033[0;32m'; YEL='\033[0;33m'; RED='\033[0;31m'; RST='\033[0m'

info()  { echo -e "${YEL}▶  $*${RST}"; }
ok()    { echo -e "${GRN}✓  $*${RST}"; }
err()   { echo -e "${RED}✗  $*${RST}"; exit 1; }

if [[ "${1:-}" == "clean" ]]; then
    info "Cleaning build artefacts…"
    rm -f blockchain_test blockchain.js blockchain.wasm
    ok "Done."
    exit 0
fi

if [[ "${1:-}" == "wasm" ]]; then
    info "Checking for Emscripten…"
    if ! command -v emcc &>/dev/null; then
        err "emcc not found. Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html"
    fi

    info "Compiling to WebAssembly…"
    mkdir -p "$(dirname ${WASM_OUT})"

    emcc blockchain.cpp bindings.cpp \
     -o "${WASM_OUT}.js" \
     -std=c++17 \
     -O2 \
     -s WASM=1 \
     -s EXPORTED_RUNTIME_METHODS='["cwrap","ccall","UTF8ToString","stringToUTF8","lengthBytesUTF8","allocate","intArrayFromString","ALLOC_NORMAL"]' \
     -s EXPORTED_FUNCTIONS='["_blockchain_init","_blockchain_load","_blockchain_add_vote","_blockchain_is_valid","_blockchain_serialize","_blockchain_tally","_blockchain_size","_blockchain_merkle_root","_sha256","_free_string","_malloc","_free"]' \
     -s ALLOW_MEMORY_GROWTH=1 \
     -s MODULARIZE=1 \
     -s EXPORT_NAME="BlockchainModule" \
     -s ENVIRONMENT='node' \
     --no-entry

    ok "WASM build complete → ${WASM_OUT}.js + ${WASM_OUT}.wasm"
    exit 0
fi

# ── Native test build ────────────────────────────────────────
info "Compiling native test binary…"
g++ -std=c++17 -O2 -Wall -Wextra \
    blockchain.cpp test.cpp \
    -o "${NATIVE_OUT}"

ok "Compiled → ${NATIVE_OUT}"
info "Running tests…\n"
"${NATIVE_OUT}"