#!/usr/bin/env bash
# Usage: download-sdk.sh [out_dir] [--pinned]
#   default: fetch latest nightly, update .sdk-version
#   --pinned: fetch exact tag from .sdk-version, do not update it
set -euo pipefail

REPO="rexglue/rexglue-sdk"
OUT_DIR="${1:-sdk}"
PINNED_MODE=false
[[ "${2:-}" == "--pinned" ]] && PINNED_MODE=true

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION_FILE="$SCRIPT_DIR/../.sdk-version"

case "$(uname -m)" in
  x86_64)  PLATFORM="linux-amd64" ;;
  aarch64) PLATFORM="linux-arm64" ;;
  *) echo "Unsupported architecture: $(uname -m)" >&2; exit 1 ;;
esac

if $PINNED_MODE; then
  TARGET_TAG="$(cat "$VERSION_FILE" | tr -d '[:space:]')"
  echo "Fetching pinned release $TARGET_TAG for $PLATFORM..."
  ASSET_URL=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/tags/$TARGET_TAG" \
    | python3 -c "
import json, sys
assets = json.load(sys.stdin)['assets']
print(next(a['browser_download_url'] for a in assets if '$PLATFORM' in a['name']))
")
else
  echo "Fetching latest nightly release for $PLATFORM..."
  RELEASE=$(curl -fsSL "https://api.github.com/repos/$REPO/releases?per_page=20" \
    | python3 -c "
import json, sys
releases = json.load(sys.stdin)
nightly = next(r for r in releases if r['tag_name'].startswith('nightly-'))
print(nightly['tag_name'])
print(next(a['browser_download_url'] for a in nightly['assets'] if '$PLATFORM' in a['name']))
")
  TARGET_TAG=$(echo "$RELEASE" | head -1)
  ASSET_URL=$(echo "$RELEASE" | tail -1)
fi

INSTALLED_VERSION_FILE="$OUT_DIR/$PLATFORM/.sdk-version"
if [[ -f "$INSTALLED_VERSION_FILE" ]] && [[ "$(cat "$INSTALLED_VERSION_FILE" | tr -d '[:space:]')" == "$TARGET_TAG" ]]; then
  echo "SDK already at $TARGET_TAG. Skipping download."
else
  echo "Downloading $TARGET_TAG..."
  curl -fL "$ASSET_URL" -o /tmp/rexglue-sdk.zip

  echo "Extracting..."
  rm -rf /tmp/rexglue-sdk-tmp
  unzip -q /tmp/rexglue-sdk.zip -d /tmp/rexglue-sdk-tmp
  mkdir -p "$OUT_DIR"
  rm -rf "$OUT_DIR/$PLATFORM"
  mv /tmp/rexglue-sdk-tmp/*/ "$OUT_DIR/$PLATFORM"
  rm -f /tmp/rexglue-sdk.zip
  echo "$TARGET_TAG" > "$INSTALLED_VERSION_FILE"
  echo "SDK installed to $OUT_DIR/$PLATFORM ($TARGET_TAG)"
fi

if ! $PINNED_MODE; then
  echo "$TARGET_TAG" > "$VERSION_FILE"
  echo "Pinned version updated to $TARGET_TAG"
fi
