#!/bin/bash
# psp-bench.sh - run the two showcase benchmarks on the device and record what they measured.
#
#   PSP_DEVICE_APPROVED=1 ./tools/psp-bench.sh --label before --driver pspgu
#   PSP_DEVICE_APPROVED=1 ./tools/psp-bench.sh --label after  --driver pspgu --baseline results/before.json
#
# The pair is psp-tests/samples/psp/psp_showcase (3D) and .../psp_2d_showcase (2D). Both
# drive their animation from the frame index, so frame N is the same scene on every build,
# and both print STAT (frame time), SPLIT (work/swap) and, for 2D, the draw/bind counters
# from the link-time --wrap harness.
#
# What it records, per sample and per run: frames, avg ms, fps, work ms, swap ms, draws,
# binds and submitted vertices. Two runs per point; the script warns if they disagree by
# more than --tolerance (default 2 %).
#
# It does not guess the configuration. --rebuild rebuilds and installs libraylib.a with
# --batch / --merge; without it the installed library is used and its hash is recorded.
# The driver is switched with tools/pspgu-build.sh and restored to pspGL at the end unless
# --keep-driver is given (the toolchain's shipped state is pspGL).
#
# The guard is deliberate, like run_sample.sh: this script resets the PSP and runs code on
# it. Set PSP_DEVICE_APPROVED=1 only with the user's explicit permission for the session.

set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
export PSPDEV=${PSPDEV:-/home/psp/pspdev}
export PATH=$PSPDEV/bin:$PATH

HOSTFS=${HOSTFS:-/tmp/s3}
OUTDIR=${OUTDIR:-$ROOT/psp-tests/bench/results}
LABEL=""
DRIVER="pspgu"
RUNS=2
TOL=2.0
REBUILD=0
KEEP_DRIVER=0
BASELINE=""
BATCH=1
MERGE=1

SAMPLES="psp_showcase psp_2d_showcase"

usage() {
    sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --label)      LABEL=${2:-}; shift 2 ;;
        --driver)     DRIVER=${2:-}; shift 2 ;;
        --runs)       RUNS=${2:-2}; shift 2 ;;
        --tolerance)  TOL=${2:-2.0}; shift 2 ;;
        --out)        OUTDIR=${2:-}; shift 2 ;;
        --baseline)   BASELINE=${2:-}; shift 2 ;;
        --batch)      BATCH=${2:-1}; shift 2 ;;
        --merge)      MERGE=${2:-1}; shift 2 ;;
        --rebuild)    REBUILD=1; shift ;;
        --keep-driver) KEEP_DRIVER=1; shift ;;
        -h|--help)    usage ;;
        *) echo "unknown option: $1" >&2; usage ;;
    esac
done

if [ "${PSP_DEVICE_APPROVED:-0}" != "1" ]; then
    echo "Refusing to touch the device: set PSP_DEVICE_APPROVED=1 after the user approves the session." >&2
    exit 2
fi

command -v pspsh >/dev/null || { echo "pspsh not on PATH" >&2; exit 1; }
pgrep -x usbhostfs_pc >/dev/null || { echo "usbhostfs_pc is not running rooted at $HOSTFS" >&2; exit 1; }
[ -d "$HOSTFS" ] || { echo "missing host filesystem root $HOSTFS" >&2; exit 1; }

case "$DRIVER" in pspgu|pspgl) ;; *) echo "--driver must be pspgu or pspgl" >&2; exit 2 ;; esac

mkdir -p "$OUTDIR"
[ -n "$LABEL" ] || LABEL=$(date -u +%Y%m%dT%H%M%SZ)

restore() {
    if [ "$KEEP_DRIVER" = "0" ] && [ "$DRIVER" = "pspgu" ]; then
        "$ROOT/tools/pspgu-build.sh" --restore >/dev/null 2>&1
    fi
}
trap restore EXIT INT TERM

# ---------------------------------------------------------------------------------------
# Configuration: driver, optional library rebuild, identity
# ---------------------------------------------------------------------------------------
if [ "$DRIVER" = "pspgu" ]; then
    "$ROOT/tools/pspgu-build.sh" >/dev/null || { echo "pspgu build failed" >&2; exit 1; }
    DRIVER_HASH=$("$ROOT/tools/pspgu-build.sh" --check | awk '/content hash/ {print substr($3,1,16)}')
else
    "$ROOT/tools/pspgu-build.sh" --restore >/dev/null || { echo "pspGL restore failed" >&2; exit 1; }
    DRIVER_HASH=$("$ROOT/tools/pspgl-build.sh" --check | awk '/content hash/ {print substr($3,1,16)}')
fi

if [ "$REBUILD" = "1" ]; then
    echo "rebuilding libraylib.a (PSP_BATCH=$BATCH PSP_MERGE_DRAWS=$MERGE)"
    ( cd "$ROOT/src" && rm -f *.o platforms/*.o libraylib.a && \
      make PLATFORM=PLATFORM_PSP PSP_BATCH="$BATCH" PSP_MERGE_DRAWS="$MERGE" -j8 >/dev/null ) \
        || { echo "library build failed" >&2; exit 1; }
    cp "$ROOT/src/libraylib.a" "$PSPDEV/psp/lib/libraylib.a"
fi

LIB_HASH=$(sha256sum "$PSPDEV/psp/lib/libraylib.a" | cut -c1-16)
GIT_REV=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)
RESULT="$OUTDIR/$LABEL.json"

# ---------------------------------------------------------------------------------------
# Run one sample. Prints JSON on stdout.
# ---------------------------------------------------------------------------------------
run_sample() {
    local name=$1 frames=$2 dir="$ROOT/psp-tests/samples/psp/$1"
    local prx="$dir/$name.prx" log r

    ( cd "$dir" && rm -f *.o *.elf *.prx && make FRAMES="$frames" >/dev/null 2>&1 ) \
        || { echo "build failed: $name" >&2; return 1; }
    cp "$prx" "$HOSTFS/" || return 1

    local avg_s=0 work_s=0 swap_s=0 fps_s=0 draws_s=0 binds_s=0 verts_s=0 runs_s=0 n=0
    local prev_avg=""

    for r in $(seq 1 "$RUNS"); do
        timeout 20 pspsh -e "reset" >/dev/null
        sleep 6
        log=$( ( printf "./$name.prx\n"; sleep $(( frames/20 + 12 )) ) | timeout $(( frames/20 + 45 )) pspsh 2>&1 )

        local a w s f d b v
        a=$(printf '%s\n' "$log" | sed -n 's/.*STAT .*avg=\([0-9.]*\).*/\1/p' | tail -1)
        f=$(printf '%s\n' "$log" | sed -n 's/.*STAT .*fps=\([0-9.]*\).*/\1/p' | tail -1)
        w=$(printf '%s\n' "$log" | sed -n 's/.*SPLIT .*work=\([0-9.]*\).*/\1/p' | tail -1)
        s=$(printf '%s\n' "$log" | sed -n 's/.*SPLIT .*swap=\([0-9.]*\).*/\1/p' | tail -1)
        # Field-anchored parsing: a plain .*binds= would also match unbinds=.
        d=$(printf '%s\n' "$log" | awk '/COUNT / { a=e=0; for (i=1;i<=NF;i++) { if ($i ~ /^arrays=/) { sub("arrays=","",$i); a=$i } if ($i ~ /^drawElements=/) { sub("drawElements=","",$i); e=$i } } print a+e }' | tail -1)
        b=$(printf '%s\n' "$log" | awk '/COUNT / { for (i=1;i<=NF;i++) if ($i ~ /^binds=/) { sub("binds=","",$i); print $i } }' | tail -1)
        v=$(printf '%s\n' "$log" | awk '/COUNT / { for (i=1;i<=NF;i++) if ($i ~ /^arrayVerts=/) { sub("arrayVerts=","",$i); print $i } }' | tail -1)
        local m
        m=$(printf '%s\n' "$log" | awk '/MERGEPROBE/ { for (i=1;i<=NF;i++) if ($i ~ /^runs=/) { sub("runs=","",$i); print $i } }' | tail -1)

        if [ -z "$a" ]; then
            echo "run $r of $name produced no STAT line; last output:" >&2
            printf '%s\n' "$log" | tail -6 >&2
            return 1
        fi

        if [ -n "$prev_avg" ]; then
            awk -v a="$prev_avg" -v b="$a" -v t="$TOL" 'BEGIN { if (a > 0 && (b-a)/a*100 > t) exit 1 }' \
                || echo "  warning: $name runs differ by more than $TOL % ($prev_avg vs $a ms)" >&2
        fi
        prev_avg=$a

        avg_s=$(awk "BEGIN{print $avg_s+$a}")
        work_s=$(awk "BEGIN{print $work_s+${w:-0}}")
        swap_s=$(awk "BEGIN{print $swap_s+${s:-0}}")
        fps_s=$(awk "BEGIN{print $fps_s+${f:-0}}")
        draws_s=$(awk "BEGIN{print $draws_s+${d:-0}}")
        binds_s=$(awk "BEGIN{print $binds_s+${b:-0}}")
        verts_s=$(awk "BEGIN{print $verts_s+${v:-0}}")
        runs_s=$(awk "BEGIN{print $runs_s+${m:-0}}")
        n=$((n+1))
    done

    local avg work fps draws binds verts runs mr
    avg=$(awk "BEGIN{printf \"%.3f\", $avg_s/$n}")
    work=$(awk "BEGIN{printf \"%.3f\", $work_s/$n}")
    fps=$(awk "BEGIN{printf \"%.1f\", $fps_s/$n}")
    draws=$(awk "BEGIN{printf \"%.2f\", $draws_s/$n/$frames}")
    binds=$(awk "BEGIN{printf \"%.2f\", $binds_s/$n/$frames}")
    verts=$(awk "BEGIN{printf \"%.0f\", $verts_s/$n/$frames}")
    runs=$(awk "BEGIN{printf \"%.2f\", $runs_s/$n/$frames}")
    mr=null
    [ "$runs_s" != "0" ] && mr=$runs
    local swap
    swap=$(awk "BEGIN{printf \"%.3f\", $swap_s/$n}")

    printf '{"name":"%s","frames":%s,"avg_ms":%s,"fps":%s,"work_ms":%s,"swap_ms":%s,"draws":%s,"binds":%s,"verts":%s,"merge_runs":%s}' \
        "$name" "$frames" "$avg" "$fps" "$work" "$swap" "$draws" "$binds" "$verts" "$mr"
}

# ---------------------------------------------------------------------------------------
# Drive the two samples
# ---------------------------------------------------------------------------------------
echo "benchmark $LABEL"
echo "  driver   : $DRIVER ($DRIVER_HASH)"
echo "  library  : $LIB_HASH  (batch=$BATCH merge=$MERGE, rebuilt=$REBUILD)"
echo "  git      : $GIT_REV"
echo "  runs     : $RUNS per sample, reset between"

BODY=""
for s in $SAMPLES; do
    case "$s" in
        psp_showcase)    FRAMES=400 ;;
        psp_2d_showcase) FRAMES=300 ;;
    esac
    echo "  running  : $s ($FRAMES frames)"
    j=$(run_sample "$s" "$FRAMES") || exit 1
    BODY="$BODY${BODY:+,}$j"
done

cat > "$RESULT" <<JSON
{
  "label": "$LABEL",
  "utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "git": "$GIT_REV",
  "driver": "$DRIVER",
  "driver_hash": "$DRIVER_HASH",
  "lib_hash": "$LIB_HASH",
  "lib_batch": $( [ "$REBUILD" = 1 ] && echo "$BATCH" || echo null ),
  "lib_merge": $( [ "$REBUILD" = 1 ] && echo "$MERGE" || echo null ),
  "rebuilt": $REBUILD,
  "samples": [$BODY]
}
JSON

echo
echo "results: $RESULT"
python3 - "$RESULT" "$TOL" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
tol = float(sys.argv[2])
print(f"{'sample':18s} {'frames':>6s} {'avg ms':>8s} {'fps':>6s} {'work ms':>8s} {'draws':>6s} {'binds':>6s} {'verts':>7s} {'runs':>5s}")
for s in d["samples"]:
    def f(k, w="8.3f"):
        v = s.get(k)
        return ("{:%s}" % w).format(v) if isinstance(v, (int, float)) else "-"
    print(f"{s['name']:18s} {s.get('frames','-'):>6} {f('avg_ms'):>8s} {f('fps','6.1f'):>6s} {f('work_ms'):>8s} {f('draws','6.1f'):>6s} {f('binds','6.1f'):>6s} {f('verts','7.0f'):>7s} {f('merge_runs','5.2f'):>5s}")
PY

# ---------------------------------------------------------------------------------------
# Optional comparison against a previous result
# ---------------------------------------------------------------------------------------
if [ -n "$BASELINE" ]; then
    [ -f "$BASELINE" ] || { echo "baseline not found: $BASELINE" >&2; exit 1; }
    echo
    echo "against $(basename "$BASELINE"):"
    python3 - "$BASELINE" "$RESULT" <<'PY'
import json, sys
base = {s["name"]: s for s in json.load(open(sys.argv[1]))["samples"]}
cur = {s["name"]: s for s in json.load(open(sys.argv[2]))["samples"]}
print(f"{'sample':18s} {'avg ms':>18s} {'fps':>14s} {'work ms':>18s}")
for name in cur:
    a, b = base.get(name), cur[name]
    if not a:
        continue
    def d(k, fmt="%+.3f"):
        if isinstance(a.get(k), (int, float)) and isinstance(b.get(k), (int, float)):
            return fmt % (b[k] - a[k])
        return "-"
    def pct(k):
        if isinstance(a.get(k), (int, float)) and a.get(k):
            return "%+.1f%%" % ((b[k] - a[k])/a[k]*100.0)
        return "-"
    print(f"{name:18s} {a.get('avg_ms'):>8.3f} -> {b.get('avg_ms'):>8.3f} {d('avg_ms'):>8s} "
          f"{a.get('fps'):>6.1f} -> {b.get('fps'):>6.1f} {pct('fps'):>9s} "
          f"{(a.get('work_ms') or 0):>8.3f} -> {(b.get('work_ms') or 0):>8.3f} {d('work_ms'):>8s}")
PY
fi

echo
echo "driver left as: $( [ "$KEEP_DRIVER" = 1 ] && echo "$DRIVER (kept)" || echo "pspGL (shipped)" )"
