#!/bin/bash
# psp-lane.sh - pin one console lane (port, host root, toolchain) and run code on it.
#
# Two consoles can hang off one PC, and more than one PSPLink client can be alive at once.
# A lane is the tuple that must not be shared: the console's port, the host0 root that
# bridge serves, and the toolchain whose psp/lib the session installs into. Sharing the
# first two means two clients drive one console; sharing the third means one session links
# the other session's library without knowing it.
#
#   ./tools/psp-lane.sh --lane a check          # is this lane usable, and what is in psp/lib?
#   eval "$(./tools/psp-lane.sh --lane a env)"  # PSPDEV, PORT, HOSTFS in this shell
#   ./tools/psp-lane.sh --lane a run foo.prx 20 # reset, run, wait, print the console output
#   ./tools/psp-lane.sh --lane a install        # copy src/libraylib.a and the pspgu driver in
#   ./tools/psp-lane.sh --lane a up|down        # start/stop this lane's usbhostfs_pc
#
# Lanes as configured here:
#   a  PSP-1000, 32 MB RAM   port 11000  root /home/psp/psp-lanes/p1000  PSPDEV /home/psp/pspdev
#   b  PSP Street (E1000), 64 MB RAM  port 10000  root /home/psp/psp-lanes/street  PSPDEV /home/psp/pspdev-street
#
# The bridge is started with the root as an explicit argument, never from a working directory:
# a usbhostfs_pc started in the wrong cwd serves the wrong tree with no error, and every
# "not found" then looks like a console fault.
#
# The overlay lines PSP_LANE_A_* / PSP_LANE_B_* below are the only place to change that.
# The guard is deliberate: a second pspsh on the lane's port makes a timing run silently
# wrong, and a run that "works" against the other lane's toolchain is not reproducible.

set -u

LANE_A_NAME="p1000"
LANE_A_PID=0x1C9
LANE_A_PORT=11000
LANE_A_ROOT=/home/psp/psp-lanes/p1000
LANE_A_PSPDEV=/home/psp/pspdev

LANE_B_NAME="street"
LANE_B_PID=0x1CA
LANE_B_PORT=10000
LANE_B_ROOT=/home/psp/psp-lanes/street
LANE_B_PSPDEV=/home/psp/pspdev-street

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LANE=""
CMD=""

usage() { sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

while [ $# -gt 0 ]; do
    case "$1" in
        --lane) LANE=${2:-}; shift 2 ;;
        env|check|run|install|up|down) CMD=$1; shift ;;
        -h|--help) usage ;;
        *) break ;;
    esac
done

case "$LANE" in
    a|A) NAME=$LANE_A_NAME; PID=$LANE_A_PID; PORT=$LANE_A_PORT; HOSTFS=$LANE_A_ROOT; PSPDEV_LANE=$LANE_A_PSPDEV ;;
    b|B) NAME=$LANE_B_NAME; PID=$LANE_B_PID; PORT=$LANE_B_PORT; HOSTFS=$LANE_B_ROOT; PSPDEV_LANE=$LANE_B_PSPDEV ;;
    *) echo "psp-lane: --lane a|b is required" >&2; exit 2 ;;
esac

export PSPDEV=$PSPDEV_LANE
export PATH=$PSPDEV_LANE/bin:$PATH

# A client on this port means someone else is driving this console right now.
client_on_port() { pgrep -ax pspsh 2>/dev/null | grep -q -- "-p $PORT\b"; }

bridge_on_port() { pgrep -af usbhostfs_pc 2>/dev/null | grep -q -- "-b $PORT\b"; }

need_quiet() {
    if client_on_port; then
        echo "psp-lane: another pspsh is attached to port $PORT ($NAME); refusing" >&2
        exit 3
    fi
}

case "$CMD" in
    env)
        echo "export PSPDEV=$PSPDEV_LANE"
        echo "export PSP_LANE=$LANE"
        echo "export PSP_LANE_NAME=$NAME"
        echo "export PSP_PORT=$PORT"
        echo "export HOSTFS=$HOSTFS"
        ;;
    check)
        echo "lane     : $LANE ($NAME)"
        echo "port     : $PORT"
        echo "root     : $HOSTFS"
        echo "toolchain: $PSPDEV_LANE"
        [ -d "$PSPDEV_LANE/psp/lib" ] || { echo "MISSING: $PSPDEV_LANE/psp/lib" >&2; exit 1; }
        [ -d "$HOSTFS" ] || { echo "MISSING: $HOSTFS" >&2; exit 1; }
        bridge_on_port || { echo "MISSING: no usbhostfs_pc serving -b $PORT (run: --lane $LANE up)" >&2; exit 1; }
        served=$(pgrep -af usbhostfs_pc 2>/dev/null | awk -v pat="-b $PORT" 'index($0, pat) {print}' | sed 's/.*usbhostfs_pc//')
        case "$served" in
            *"$HOSTFS"*) echo "serving  : $HOSTFS" ;;
            *) echo "serving  : WARNING - bridge not started with the explicit root (cmdline:$served)" ;;
        esac
        if client_on_port; then echo "client   : ATTACHED (a run would collide)"; else echo "client   : none"; fi
        echo -n "libraylib: "; sha256sum "$PSPDEV_LANE/psp/lib/libraylib.a" 2>/dev/null | cut -c1-16 || echo missing
        echo -n "libGL    : "; sha256sum "$PSPDEV_LANE/psp/lib/libGL.a" 2>/dev/null | cut -c1-16 || echo missing
        ;;
    install)
        need_quiet
        [ -f "$ROOT/src/libraylib.a" ] || { echo "psp-lane: no src/libraylib.a to install" >&2; exit 1; }
        flock /tmp/psp-lane-$PORT.lock -c \
            "cp '$ROOT/src/libraylib.a' '$PSPDEV_LANE/psp/lib/libraylib.a' && PSPDEV='$PSPDEV_LANE' '$ROOT/tools/pspgu-build.sh'" \
            || { echo "psp-lane: install failed" >&2; exit 1; }
        echo "installed into $PSPDEV_LANE/psp/lib"
        ;;
    run)
        PRX=${1:-}; SLIP=${2:-14}
        [ -n "$PRX" ] || { usage; }
        [ -f "$HOSTFS/$PRX" ] || [ -f "$PRX" ] || { echo "psp-lane: $PRX not found in $HOSTFS" >&2; exit 1; }
        need_quiet
        exec flock /tmp/psp-lane-$PORT.lock -c \
            "timeout 20 pspsh -p $PORT -e reset >/dev/null 2>&1; sleep 9; \
             ( printf './$PRX\n'; sleep $SLIP ) | timeout $((SLIP + 30)) pspsh -p $PORT 2>&1"
        ;;
    up)
        if bridge_on_port; then echo "bridge already serving -b $PORT"; exit 0; fi
        [ -d "$HOSTFS" ] || { echo "psp-lane: missing root $HOSTFS" >&2; exit 1; }
        setsid nohup usbhostfs_pc -p "$PID" -b "$PORT" "$HOSTFS" > "/tmp/usbhostfs-$PORT.log" 2>&1 &
        sleep 1
        bridge_on_port && echo "bridge up: -p $PID -b $PORT root $HOSTFS" || { echo "psp-lane: bridge failed to start" >&2; exit 1; }
        ;;
    down)
        pids=$(pgrep -af usbhostfs_pc 2>/dev/null | awk -v pat="-b $PORT" 'index($0, pat) {print $1}')
        if [ -n "$pids" ]; then echo "$pids" | xargs -r kill; echo "bridge stopped for lane $LANE"; else echo "no bridge on -b $PORT"; fi
        ;;
    *)
        usage
        ;;
esac
