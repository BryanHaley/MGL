#!/bin/bash
# Measure several CTS groups in sequence, chunked.
for g in "$@"; do
  out=/tmp/cts-$(echo "$g" | tr -c 'A-Za-z0-9' '_')
  ./scratchpad/cts_run.sh "$g" 200 "$out" 900 2>/dev/null | tail -4
done
