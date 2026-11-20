#!/bin/bash
# Sprint 008 close-out probes: -O2 pixel check, then the -O2 unbatched baseline.
set -u
export PSPDEV=/home/psp/pspdev
export PATH=/home/psp/pspdev/bin:$PATH
BC=/home/psp/raylib-psp/psp-tests/batch_check

echo "=== 1. batch_check under -O2 (sprite arm), compare against the -O0 capture"
cp /tmp/arm3/libraylib.a $PSPDEV/psp/lib/libraylib.a
cd $BC || exit 1
rm -f *.o batch_check.prx
make >/tmp/bc_o2.log 2>&1 || { echo "BUILD FAILED"; exit 1; }
cp batch_check.prx /tmp/s3/bco2.prx
timeout 20 pspsh -e "reset" >/dev/null 2>&1; sleep 6
(cd /tmp/s3 && ( printf './bco2.prx\n'; sleep 12 ) | timeout 30 pspsh 2>&1 | grep -E "COUNT|DONE")
timeout 25 pspsh -e "scrshot host0:/co2.raw" >/dev/null 2>&1
echo "  -O2 capture md5: $(md5sum /tmp/s3/co2.raw | cut -d' ' -f1)"
echo "  diff bytes vs -O0: $(cmp -l /tmp/s3/co2.raw /tmp/s3/bc4b.raw 2>/dev/null | wc -l)"

echo "=== 2. -O2 unbatched baseline"
echo "#### arm5 (-O2 unbatched)"
/home/psp/raylib-psp/psp-tests/bunnymark/measure.sh 5 2>&1 | grep -E "ARM|STAT|SPLIT|COUNT|FAILED"
