#!/bin/bash
# Measure the PSP_BATCH arms: N in {100,1000,5000}, two runs each, uncapped.
# Usage: measure.sh <arm 0|1>
set -u
export PSPDEV=/home/psp/pspdev
export PATH=/home/psp/pspdev/bin:$PATH
ARM=$1
FRAMES=200
SLEEP=45

cp /tmp/arm${ARM}/libraylib.a $PSPDEV/psp/lib/libraylib.a
cd /home/psp/raylib-psp/psp-tests/bunnymark || exit 1

for N in 100 1000 5000; do
    make clean >/dev/null 2>&1
    if ! make BUNNY_COUNT=$N FRAMES=$FRAMES TARGET_FPS=0 >/tmp/build_bm_${ARM}_${N}.log 2>&1; then
        echo "ARM$ARM N=$N BUILD FAILED"; continue
    fi
    cp bunnymark.prx /tmp/s3/app.prx
    for RUN in 1 2; do
        timeout 20 pspsh -e "reset" >/dev/null 2>&1
        sleep 6
        OUT=$(cd /tmp/s3 && ( printf './app.prx\n'; sleep $SLEEP ) | timeout 90 pspsh 2>&1)
        STAT=$(echo "$OUT" | grep "STAT n=" | tail -1)
        SPLIT=$(echo "$OUT" | grep "SPLIT" | tail -1)
        CNT=$(echo "$OUT" | grep "COUNT done" | tail -1)
        DONE=$(echo "$OUT" | grep -c "DONE frames")
        echo "ARM$ARM N=$N RUN=$RUN done=$DONE"
        echo "  $CNT"
        echo "  $STAT"
        echo "  $SPLIT"
    done
done
