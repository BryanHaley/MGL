#!/bin/bash
# Chunked CTS runner. A hang or crash costs one chunk, not the measurement.
# usage: cts_run.sh <case-pattern> [chunk-size] [outdir] [per-chunk-timeout]
set -u
MOD=/Users/bryan/Projects/MGL/external/VK-GL-CTS/build/external/openglcts/modules
PAT="${1:?pattern}"
CHUNK="${2:-100}"
OUT="${3:-/tmp/cts-out}"
TMO="${4:-600}"

rm -rf "$OUT"; mkdir -p "$OUT"
cd "$MOD" || exit 1
./glcts --deqp-runmode=stdout-caselist --deqp-case="$PAT" 2>/dev/null \
  | sed -n 's/^TEST: //p' > "$OUT/cases.txt"

total=$(wc -l < "$OUT/cases.txt" | tr -d ' ')
echo "$total cases matching $PAT"
split -l "$CHUNK" "$OUT/cases.txt" "$OUT/chunk."

for c in "$OUT"/chunk.*; do
  case "$c" in *.qpa|*.log) continue;; esac
  timeout "$TMO" ./glcts --deqp-caselist-file="$c" --deqp-runmode=execute \
      --deqp-log-filename="$c.qpa" >"$c.log" 2>&1
  printf '%s rc=%s\n' "$(basename "$c")" "$?" >> "$OUT/rc.txt"
done

ran=$(cat "$OUT"/chunk.*.qpa 2>/dev/null | grep -c 'StatusCode=')
echo "=== $PAT ==="
echo "cases: $total   reached a verdict: $ran"
cat "$OUT"/chunk.*.qpa 2>/dev/null | grep -o 'StatusCode="[A-Za-z]*"' | sort | uniq -c | sort -rn
