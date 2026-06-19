#!/usr/bin/env bash
# Generate realistically-sized dummy cooked content so pb_unreal's
# "large asset" findings (Medium >25 MiB, High >100 MiB) fire on the sample.
# The repo only ships tiny placeholders; run this to inflate them locally.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p Content/Maps
head -c 120000000 /dev/zero > Content/Textures_big.uasset   # 114 MiB -> HIGH
head -c 30000000  /dev/zero > Content/Characters.uasset      # 28 MiB  -> MEDIUM
head -c 2000000   /dev/zero > Content/Maps/Level1.umap       # ~2 MiB  -> LOW
echo "Generated dummy cooked content under Content/"
