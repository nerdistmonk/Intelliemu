#!/bin/sh
# Regression checks for the jzutils tools.
# Usage: test.sh [bindir]   (default: build/bin next to this script)

here=$(cd "$(dirname "$0")" && pwd)
bin=${1:-$here/build/bin}
if [ ! -x "$bin/as1600" ]; then
    echo "no jzutils tools found in $bin" >&2
    exit 2
fi

tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT

pass=0
fail=0

ok()   { pass=$((pass + 1)); echo "ok   $1"; }
bad()  { fail=$((fail + 1)); echo "FAIL $1"; }

# CRC-32 of "123456789" is the standard check value CBF43926.
printf '123456789' > "$tmp/crc.bin"
if "$bin/crc32" "$tmp/crc.bin" | grep -q CBF43926; then
    ok "crc32 known answer"
else
    bad "crc32 known answer"
fi

if "$bin/crc32" "$tmp/does-not-exist.bin" >/dev/null 2>&1; then
    bad "crc32 missing file should fail"
else
    ok "crc32 missing file fails"
fi

cat > "$tmp/t.asm" <<'EOF'
        ORG $5000
start:  MVII #$1234, R0
        HLT
        DECLE $ABCD
EOF

# Golden output for the sample above, produced by this build.
golden_as='6de69dcdc3bb47d324ed2fb36fadc35a70ec9090eb76b9766f0a8024f6bb39e0'
if "$bin/as1600" -o "$tmp/t.bin" "$tmp/t.asm" >/dev/null 2>&1 &&
   [ "$(sha256sum "$tmp/t.bin" | cut -d' ' -f1)" = "$golden_as" ]; then
    ok "as1600 golden output"
else
    bad "as1600 golden output"
fi

if "$bin/dasm1600" -d "$tmp/t.bin" 2>/dev/null | grep -q 'MVII.*#\$1234' &&
   "$bin/dasm1600" -d "$tmp/t.bin" 2>/dev/null | grep -q 'HLT'; then
    ok "dasm1600 decodes assembled output"
else
    bad "dasm1600 decodes assembled output"
fi

mkdir "$tmp/rom"
cp "$tmp/t.bin" "$tmp/rom/p.bin"
cd "$tmp/rom" || exit 2
if "$bin/bin2rom" --cc3 p.bin >/dev/null 2>&1 && [ -s p.rom ]; then
    ok "bin2rom --cc3"
else
    bad "bin2rom --cc3"
fi

mkdir "$tmp/back"
cp p.rom "$tmp/back/"
cd "$tmp/back" || exit 2
if "$bin/rom2bin" p.rom >/dev/null 2>&1 && cmp -s -n 8 p.bin "$tmp/t.bin"; then
    ok "rom2bin round trip keeps code bytes"
else
    bad "rom2bin round trip keeps code bytes"
fi

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
