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

CLI_PATH="$REPO_ROOT/$OUTPUT_DIR/apidetect"
GUI_PATH="$REPO_ROOT/$OUTPUT_DIR/apidetect-gui"
"$GO_BIN" build -trimpath -o "$CLI_PATH" ./cmd/apidetect
"$GO_BIN" build -trimpath -o "$GUI_PATH" ./cmd/apidetect-gui

echo
echo "Build completed:"
echo "  $CLI_PATH"
echo "  $GUI_PATH"
