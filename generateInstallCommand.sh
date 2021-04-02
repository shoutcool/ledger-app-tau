#!/usr/bin/env bash

appVersion=$(grep "APPVERSION =" Makefile | cut -d '=' -f2)
dataSize=$((0x`cat debug/app.map |grep _envram_data | tr -s ' ' | cut -f2 -d' '|cut -f2 -d'x'` - 0x`cat debug/app.map |grep _nvram_data | tr -s ' ' | cut -f2 -d' '|cut -f2 -d'x'`))
iconHex=$(python3 ~/ledger/nanos-secure-sdk/icon3.py --hexbitmaponly lamden.gif  2>/dev/null)
installCmd=$(printf 'python -m ledgerblue.loadApp \
    --appFlags 0x40 \
    --path "44\x27/789\x27" \
    --curve ed25519 \
    --tlv \
    --targetId 0x31100004 \
    --targetVersion="1.6.1" \
    --delete \
    --fileName bin/app.hex \
    --appName Lamden \
    --appVersion %s \
    --dataSize %s \
    --icon %s' "$appVersion" "$dataSize" "$iconHex")

echo "$installCmd"


