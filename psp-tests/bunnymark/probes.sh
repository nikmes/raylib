#!/bin/bash
# Extra probes on one already-staged libraylib.a: fill vs vertices, and the HUD floor.
# Usage: probes.sh <tag:make-args-string>
#   probes.sh scale2 "BUNNY_COUNT=1000 SPRITE_SCALE=2"
#   probes.sh skip   "BUNNY_COUNT=1000 SKIP_DRAW=1"
set -u
export PSPDEV=/home/psp/pspdev
export PATH=/home/psp/pspdev/bin:$PATH
TAG=$1
ARGS=$2
FRAMES=200
SLEEP=45

cd /home/psp/raylib-psp/psp-tests/bunnymark || exit 1
make clean >/dev/null 2>&1
if ! make $ARGS FRAMES=$FRAMES TARGET_FPS=0 >/tmp/build_${TAG}.log 2>&1; then
    echo "$TAG BUILD FAILED"; exit 1
fi
cp bunnymark.prx /tmp/s3/app.prx
for RUN in 1 2; do
    timeout 20 pspsh -e "reset" >/dev/null 2>&1
    sleep 6
    OUT=$(cd /tmp/s3 && ( printf './app.prx\n'; sleep $SLEEP ) | timeout 90 pspsh 2>&1)
    echo "PROBE $TAG RUN=$RUN done=$(echo "$OUT" | grep -c 'DONE frames')"
    echo "  $(echo "$OUT" | grep 'SPAWN' | tail -1)"
    echo "  $(echo "$OUT" | grep 'COUNT done' | tail -1)"
    echo "  $(echo "$OUT" | grep 'STAT n=' | tail -1)"
    echo "  $(echo "$OUT" | grep 'SPLIT' | tail -1)"
done
