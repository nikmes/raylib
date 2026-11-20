#!/bin/bash
# Remaining sprint 007 probes, one device session:
#   1. unbatched N=5000 (short run: 100 frames, longer window)
#   2. batched, 4x fill       -> separates fill cost from vertex/submission cost
#   3. batched, no sprites    -> the HUD floor
#   4. batched + glLockArraysEXT
set -u
export PSPDEV=/home/psp/pspdev
export PATH=/home/psp/pspdev/bin:$PATH
BM=/home/psp/raylib-psp/psp-tests/bunnymark
STAGE=$PSPDEV/psp/lib/libraylib.a

run_app() {   # run_app <sleep-seconds>
    timeout 20 pspsh -e "reset" >/dev/null 2>&1
    sleep 6
    cd /tmp/s3 && ( printf './app.prx\n'; sleep $1 ) | timeout 200 pspsh 2>&1
}

one_config() {  # one_config <tag> <frames> <sleep> <make args>
    local tag=$1 frames=$2 slp=$3; shift 3
    cd $BM || exit 1
    make clean >/dev/null 2>&1
    if ! make "$@" FRAMES=$frames TARGET_FPS=0 >/tmp/build_$tag.log 2>&1; then
        echo "PROBE $tag BUILD FAILED"; return
    fi
    cp bunnymark.prx /tmp/s3/app.prx
    for RUN in 1 2; do
        OUT=$(run_app $slp)
        echo "PROBE $tag RUN=$RUN done=$(echo "$OUT" | grep -c 'DONE frames')"
        echo "  $(echo "$OUT" | grep 'SPAWN' | tail -1)"
        echo "  $(echo "$OUT" | grep 'COUNT done' | tail -1)"
        echo "  $(echo "$OUT" | grep 'STAT n=' | tail -1)"
        echo "  $(echo "$OUT" | grep 'SPLIT' | tail -1)"
    done
}

echo "=== 1. arm0 N=5000 (100 frames)"
cp /tmp/arm0/libraylib.a $STAGE
one_config arm0_n5000 100 80 BUNNY_COUNT=5000

echo "=== 2-4. arm1 probes"
cp /tmp/arm1/libraylib.a $STAGE
one_config scale2 200 45 BUNNY_COUNT=1000 SPRITE_SCALE=2
one_config skip   200 45 BUNNY_COUNT=1000 SKIP_DRAW=1

echo "=== 4. lock arrays"
cd /home/psp/raylib-psp/src
make clean >/dev/null 2>&1
make PLATFORM=PLATFORM_PSP PSP_BATCH=1 PSP_BATCH_LOCK=1 -j8 >/tmp/build_lock.log 2>&1
if grep -q "error:" /tmp/build_lock.log; then echo "LOCK BUILD FAILED"; else
    cp libraylib.a $STAGE
    one_config lock 200 45 BUNNY_COUNT=1000
fi

# leave the plain batched library staged
cp /tmp/arm1/libraylib.a $STAGE
echo "=== done, arm1 restaged"
