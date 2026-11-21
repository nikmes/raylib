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
    if [ -f "$LIB" ] && ! grep -q "pspgu" <(psp-nm "$LIB" 2>/dev/null | grep -c pspguBeginFrame) 2>/dev/null; then
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
        if [ -f "$BACKUP" ]; then
            cp "$BACKUP" "$LIB"; echo "restored the pspGL archive from $BACKUP"
        elif [ -f "$PSPGL_SRC/libGL.a" ]; then
            cp "$PSPGL_SRC/libGL.a" "$LIB"; echo "restored pspGL from $PSPGL_SRC"
        else
            echo "no pspGL archive to restore; run tools/pspgl-build.sh"; exit 1
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
