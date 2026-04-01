#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GO_BIN="${GO_BIN:-go}"

if ! command -v "$GO_BIN" >/dev/null 2>&1; then
  echo "Go toolchain not found: $GO_BIN" >&2
  exit 1
fi

GOOS_VALUE="$("$GO_BIN" env GOOS)"
GOARCH_VALUE="$("$GO_BIN" env GOARCH)"
OUTPUT_DIR="${1:-dist/${GOOS_VALUE}-${GOARCH_VALUE}}"

mkdir -p "$REPO_ROOT/.cache/go-build" "$REPO_ROOT/.cache/gomod" "$REPO_ROOT/$OUTPUT_DIR"

export GOCACHE="$REPO_ROOT/.cache/go-build"
export GOMODCACHE="$REPO_ROOT/.cache/gomod"

echo "Using Go: $GO_BIN"
"$GO_BIN" version

BINARY_PATH="$REPO_ROOT/$OUTPUT_DIR/apidetect"
"$GO_BIN" build -trimpath -o "$BINARY_PATH" ./cmd/apidetect

echo
echo "Build completed:"
echo "  $BINARY_PATH"
