#!/bin/bash
set -euo pipefail

SRC_DIR="${SRC_DIR:-/mnt/examples-src}"
DEST_DIR="${DEST_DIR:-/home/examples}"

usage() {
  cat <<'EOF'
Usage:
  /root/sync_examples.sh

Environment:
  SRC_DIR   Source directory with examples (default: /mnt/examples-src)
  DEST_DIR  Destination directory inside /home volume (default: /home/examples)

Notes:
  - If SRC_DIR is not present (e.g. no bind-mount), script exits successfully.
  - Sync is mirror-like: files removed from SRC_DIR are removed from DEST_DIR.
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
  echo "sync_examples: must be run as root (use sudo)." >&2
  exit 1
fi

if [ ! -d "$SRC_DIR" ]; then
  echo "sync_examples: source '$SRC_DIR' not found; skipping (ok)." >&2
  exit 0
fi

# Ensure destination exists in the persisted /home volume
mkdir -p "$DEST_DIR"

# Prefer stable, readable permissions for all users.
# Dirs: 755, Files: 644
# --delete: mirror semantics
echo "sync_examples: syncing '$SRC_DIR/' -> '$DEST_DIR/' (mirror)"
rsync -a --delete \
  --chmod=Du=rwx,Dgo=rx,Fu=rw,Fgo=r \
  --numeric-ids \
  "$SRC_DIR/" "$DEST_DIR/"

# Also ensure ownership is root:root (safe default in shared /home)
chown -R root:root "$DEST_DIR" || true

echo "sync_examples: done"
