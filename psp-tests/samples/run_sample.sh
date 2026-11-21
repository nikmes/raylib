#!/bin/bash
# Run one ported sample on the PSP and archive two identical captures.
#
#   PSP_DEVICE_APPROVED=1 ./run_sample.sh textures/image_kernel [run_seconds] [evidence_dir]
#
# The guard is deliberate: this script starts PSPLink (pspsh) and touches the
# device. Do not set PSP_DEVICE_APPROVED=1 without the user's explicit permission.
#
# STAGE_ASSETS=1 copies examples/textures/resources into /tmp/s3/resources first.
set -u

if [ "${PSP_DEVICE_APPROVED:-0}" != "1" ]; then
    echo "Refusing to start PSPLink: set PSP_DEVICE_APPROVED=1 after the user approves the device session." >&2
    exit 2
fi

export PSPDEV=/home/psp/pspdev
export PATH=$PSPDEV/bin:$PATH

SAMPLE=${1:?usage: run_sample.sh <category>/<name> [run_seconds] [evidence_dir]}
RUN=${2:-25}
EVID=${3:-/home/psp/evidence/run}
NAME=$(basename "$SAMPLE")
PRX="/home/psp/raylib-psp/psp-tests/samples/$SAMPLE/$NAME.prx"
HOSTFS=/tmp/s3

if [ ! -f "$PRX" ]; then echo "missing PRX: $PRX" >&2; exit 1; fi
if ! pgrep -x usbhostfs_pc >/dev/null 2>&1; then
    echo "usbhostfs_pc is not running; start it yourself (this script will not)." >&2
    exit 1
fi
mkdir -p "$EVID"

if [ "${STAGE_ASSETS:-0}" = "1" ]; then
    mkdir -p "$HOSTFS/resources"
    cp /home/psp/raylib-psp/examples/textures/resources/*.png \
       /home/psp/raylib-psp/examples/textures/resources/*.gif \
       /home/psp/raylib-psp/examples/textures/resources/*.ttf \
       /home/psp/raylib-psp/examples/textures/resources/*.raw \
       "$HOSTFS/resources/" 2>/dev/null || true
fi

capture() {
    local slot=$1
    local raw="$HOSTFS/${NAME}_${slot}.raw"
    local attempt
    for attempt in 1 2 3; do
        rm -f "$raw"
        timeout 15 pspsh -e "scrshot host0:/${NAME}_${slot}.raw" >/dev/null 2>&1
        sleep 3
        if [ -s "$raw" ] && [ "$(stat -c%s "$raw")" -eq 391734 ]; then return 0; fi
        echo "  capture $slot attempt $attempt incomplete"
    done
    return 1
}

cp "$PRX" "$HOSTFS/app.prx"
timeout 20 pspsh -e "reset" >/dev/null 2>&1
sleep 6

( cd "$HOSTFS" && ( printf './app.prx\n'; sleep "$RUN" ) | timeout $((RUN + 20)) pspsh ) \
    > "$EVID/$NAME.log" 2>&1

IDENT=no
for pair in 1 2 3; do
    capture 1 && capture 2 || continue
    sleep 2
    if cmp -s "$HOSTFS/${NAME}_1.raw" "$HOSTFS/${NAME}_2.raw"; then IDENT=yes; break; fi
    echo "  captures differ, retrying"
done

cp "$HOSTFS/${NAME}_1.raw" "$EVID/${NAME}_1.bmp" 2>/dev/null
cp "$HOSTFS/${NAME}_2.raw" "$EVID/${NAME}_2.bmp" 2>/dev/null
echo "$NAME done=$(grep -c 'DONE frames 600' "$EVID/$NAME.log") identical=$IDENT"
