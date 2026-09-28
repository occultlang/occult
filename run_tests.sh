#!/usr/bin/env bash
# Linux port of validate_tests.ps1. Same classification rules, 5s timeout.
# Usage: run_tests.sh <occultc> <outdir>   (run from the occult project root)
COMPILER="$1"; OUT="$2"
mkdir -p "$OUT"
: > "$OUT/summary.txt"
pass=0; fail=0; skip=0
for f in tests/*.occ; do
  name=$(basename "$f")
  if [ "$name" = "test.occ" ]; then echo "SKIP $name" >> "$OUT/summary.txt"; skip=$((skip+1)); continue; fi
  timeout 5 "$COMPILER" "$f" > "$OUT/$name.stdout" 2> "$OUT/$name.stderr"
  rc=$?
  err=$(tr '\n' ' ' < "$OUT/$name.stderr")
  if [ $rc -eq 124 ]; then status="TIMEOUT"
  elif echo "$err" | grep -qE "PARSE ERROR|Function '[^']*' not found|Function \"[^\"]*\" not found"; then status="FAIL(compile)"
  elif [ $rc -eq 139 ] || echo "$err" | grep -q "Segmentation fault"; then status="FAIL(segv)"
  elif [ $rc -ne 0 ]; then status="FAIL(rc=$rc)"
  else status="PASS"; fi
  if head -1 "$f" | grep -q "EXPECT-FAIL"; then
    if [ $rc -ne 0 ] && [ $rc -ne 124 ] && [ $rc -ne 139 ]; then status="PASS"; else status="FAIL(expected-error)"; fi
  fi
  if [ "$status" = "PASS" ]; then pass=$((pass+1)); else fail=$((fail+1)); fi
  echo "$status $name" >> "$OUT/summary.txt"
done
echo "pass=$pass fail=$fail skip=$skip" | tee -a "$OUT/summary.txt"
