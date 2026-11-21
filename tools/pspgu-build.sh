#!/bin/bash
# Build and install the pspgu driver (phase 5): a GE driver that supplies the GL 1.1
# subset raylib's GL11 path needs, replacing pspGL.
#
#   tools/pspgu-build.sh                 # build, install over libGL.a, keep the old one
#   tools/pspgu-build.sh --depth=0       # 2D-only build, 272 KiB of VRAM back
#   tools/pspgu-build.sh --check         # is the installed libGL.a a pspgu build, and which?
#   tools/pspgu-build.sh --restore       # put pspGL back (rebuilds it from /home/psp/ref)
#   tools/pspgu-build.sh --verify-repeat # build twice, require the same content hash
#
# The archive's own sha256 is not stable (pspGL embeds __DATE__ in two objects); pspgu has
# no such timestamp, so a content hash over the object files is the reproducible contract.
set -u

PSPDEV="${PSPDEV:-/home/psp/pspdev}"
SRC="$(cd "$(dirname "$0")/pspgu" && pwd)"
LIB="${PSPDEV}/psp/lib/libGL.a"
BACKUP="${PSPDEV}/psp/lib/libGL.a.pspgl-backup"
PSPGL_SRC="/home/psp/ref/pspgl-themiriron2"
export PSPDEV
export PATH="${PSPDEV}/bin:${PATH}"

content_hash() {   # hash what the archive actually contains, not its bytes
    (cd "$1" && psp-nm libGL.a 2>/dev/null | sed 's/^[0-9a-f]* //' | sort)
}

build() {
    local depth="${1:-}"
    make -C "$SRC" clean >/dev/null 2>&1
    if [ -n "$depth" ]; then
        make -C "$SRC" "PSP_GU_DEPTH=${depth}" || return 1
    else
        make -C "$SRC" || return 1
    fi
    return 0
}

install_lib() {
    # Back up the previous archive only if it is NOT already pspgu. The old test
    # piped `grep -c` into `grep -q pspgu`, whose output ("1") never contains the
    # word, so every pspgu install overwrote the pspGL backup with pspgu. That cost
    # one session's A/B; test the archive's symbols directly now.
    if [ -f "$LIB" ] && ! psp-nm "$LIB" 2>/dev/null | grep -q pspguBeginFrame; then
        cp "$LIB" "$BACKUP"
    fi
    cp "$SRC/libGL.a" "$LIB"
}

case "${1---install}" in
    --check)
        if [ ! -f "$LIB" ]; then echo "no libGL.a installed"; exit 1; fi
        if psp-nm "$LIB" 2>/dev/null | grep -q pspguBeginFrame; then
            echo "installed libGL.a is a pspgu build"
            echo "content hash: $(content_hash "$(dirname "$LIB")" | sha256sum | cut -d' ' -f1)"
            psp-nm "$LIB" 2>/dev/null | grep -c "pspgl_" | xargs echo "pspGL objects inside:"
        else
            echo "installed libGL.a is NOT a pspgu build (pspGL or something else)"
            exit 1
        fi
        ;;
    --restore)
        # Only restore a real pspGL archive: an old bug could leave a pspgu build in
        # the backup slot, and restoring that silently makes every A/B a pspgu run.
        if [ -f "$BACKUP" ] && ! psp-nm "$BACKUP" 2>/dev/null | grep -q pspguBeginFrame; then
            cp "$BACKUP" "$LIB"; echo "restored the pspGL archive from $BACKUP"
        elif [ -f "$PSPGL_SRC/libGL.a" ] && ! psp-nm "$PSPGL_SRC/libGL.a" 2>/dev/null | grep -q pspguBeginFrame; then
            cp "$PSPGL_SRC/libGL.a" "$LIB"; echo "restored pspGL from $PSPGL_SRC"
        else
            echo "no valid pspGL archive to restore; run tools/pspgl-build.sh"; exit 1
        fi
        ;;
    --verify-repeat)
        build || exit 1
        a=$(content_hash "$SRC")
        build || exit 1
        b=$(content_hash "$SRC")
        if [ "$a" = "$b" ]; then echo "reproducible: $(echo "$a" | sha256sum | cut -d' ' -f1)"; else echo "NOT reproducible"; exit 1; fi
        ;;
    --depth=*)
        build "${1#--depth=}" || exit 1
        install_lib
        echo "installed pspgu with PSP_GU_DEPTH=${1#--depth=}"
        ;;
    *)
        build || exit 1
        install_lib
        echo "installed pspgu over $LIB"
        echo "  back up of the previous archive: $BACKUP"
        echo "  restore pspGL with: $0 --restore"
        ;;
esac
