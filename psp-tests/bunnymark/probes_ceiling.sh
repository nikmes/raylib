#!/bin/bash
# Sprint 008 T5: where does the 60 fps ceiling sit, sprites vs triangles, both at -O2?
# Plus the mixed sprite/triangle worst case (ROTATE_ODD).
set -u
export PSPDEV=/home/psp/pspdev
export PATH=/home/psp/pspdev/bin:$PATH
BM=/home/psp/raylib-psp/psp-tests/bunnymark

run_config() {  # run_config <tag> <arm> <frames> <sleep> <make args>
    local tag=$1 arm=$2 frames=$3 slp=$4; shift 4
    cp /tmp/arm$arm/libraylib.a $PSPDEV/psp/lib/libraylib.a
    cd $BM || exit 1
    make clean >/dev/null 2>&1
    if ! make "$@" FRAMES=$frames TARGET_FPS=0 >/tmp/build_$tag.log 2>&1; then
        echo "PROBE $tag BUILD FAILED"; return
    fi
    cp bunnymark.prx /tmp/s3/app.prx
    for RUN in 1 2; do
        timeout 20 pspsh -e "reset" >/dev/null 2>&1
        sleep 6
        OUT=$(cd /tmp/s3 && ( printf './app.prx\n'; sleep $slp ) | timeout 200 pspsh 2>&1)
        echo "PROBE $tag RUN=$RUN done=$(echo "$OUT" | grep -c 'DONE frames')"
        echo "  $(echo "$OUT" | grep 'COUNT done' | tail -1)"
        echo "  $(echo "$OUT" | grep 'STAT n=' | tail -1)"
        echo "  $(echo "$OUT" | grep 'SPLIT' | tail -1)"
    done
}

echo "=== sprites (-O2), ceiling"
run_config spr1500 3 200 45 BUNNY_COUNT=1500
run_config spr2000 3 200 45 BUNNY_COUNT=2000
echo "=== triangles (-O2), ceiling"
run_config tri1500 4 200 45 BUNNY_COUNT=1500
run_config tri2000 4 200 45 BUNNY_COUNT=2000
echo "=== mixed: every second sprite rotated"
run_config rotodd 3 200 45 BUNNY_COUNT=1000 ROTATE_ODD=1
