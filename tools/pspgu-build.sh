#!/bin/bash
# Build and install the pspgu driver (phase 5): a GE driver that supplies the GL 1.1
# subset raylib's GL11 path needs, replacing pspGL.
#
#   tools/pspgu-build.sh                 # build, install over libGL.a, keep the old one
#   tools/pspgu-build.sh --depth=0       # 2D-only build, 272 KiB of VRAM back
#   tools/pspgu-build.sh --texfmt=auto    # per-texture 16-bit formats (measured, not default)
#   PSPGU_TEXSYNC_FIRST=0 tools/pspgu-build.sh  # pre-P10-S9 TexFlush->TexSync order
#   PSPGU_FB_RESTORE=1 tools/pspgu-build.sh     # P10-S9 corrected screen-target save
#   tools/pspgu-build.sh --check         # is the installed libGL.a a pspgu build, and which?
#   tools/pspgu-build.sh --restore       # put pspGL back (rebuilds it from /home/psp/ref)
#   tools/pspgu-build.sh --verify-repeat # build twice, require the same content hash
#   tools/pspgu-build.sh --verify-install # do the installed objects match a fresh build?
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
    local args=()

    [ -n "$depth" ] && args+=("PSP_GU_DEPTH=${depth}")
    [ -n "${PSP_GU_TEXFMT:-}" ] && args+=("PSP_GU_TEXFMT=${PSP_GU_TEXFMT}")
    [ -n "${PSPGU_FAST2D:-}" ] && args+=("PSPGU_FAST2D=${PSPGU_FAST2D}")
    [ -n "${PSPGU_FAST_CLEAR:-}" ] && args+=("PSPGU_FAST_CLEAR=${PSPGU_FAST_CLEAR}")
    [ -n "${PSPGU_VFPU:-}" ] && args+=("PSPGU_VFPU=${PSPGU_VFPU}")
    [ -n "${PSPGU_VFPU_CLIP:-}" ] && args+=("PSPGU_VFPU_CLIP=${PSPGU_VFPU_CLIP}")
    [ -n "${PSPGU_REPLAY:-}" ] && args+=("PSPGU_REPLAY=${PSPGU_REPLAY}")
    [ -n "${PSPGU_LIST_VERTICES:-}" ] && args+=("PSPGU_LIST_VERTICES=${PSPGU_LIST_VERTICES}")
    [ -n "${PSPGU_SUBMIT:-}" ] && args+=("PSPGU_SUBMIT=${PSPGU_SUBMIT}")
    [ -n "${PSPGU_CLIP_PLANES:-}" ] && args+=("PSPGU_CLIP_PLANES=${PSPGU_CLIP_PLANES}")
    [ -n "${PSPGU_DITHER_ADDITIVE:-}" ] && args+=("PSPGU_DITHER_ADDITIVE=${PSPGU_DITHER_ADDITIVE}")
    [ -n "${PSPGU_TEXSYNC_FIRST:-}" ] && args+=("PSPGU_TEXSYNC_FIRST=${PSPGU_TEXSYNC_FIRST}")
    [ -n "${PSPGU_FB_RESTORE:-}" ] && args+=("PSPGU_FB_RESTORE=${PSPGU_FB_RESTORE}")

    # The Makefile's clean has been observed to leave objects behind on some checkouts;
    # a stale .o silently produces a false A/B (sprint 079 §9). Remove them explicitly.
    rm -f "$SRC"/*.o "$SRC"/libGL.a
    make -C "$SRC" clean >/dev/null 2>&1
    make -C "$SRC" "${args[@]}" || return 1
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
    --verify-install)
        # A stale archive on the device has produced wrong measurements more than once.
        # Compare object bytes, not the symbol-list content hash: a code change does not
        # alter the symbols, which is exactly how a stale install goes unnoticed.
        if ! psp-nm "$LIB" 2>/dev/null | grep -q pspguBeginFrame; then
            echo "installed libGL.a is not a pspgu build"; exit 1
        fi
        build || exit 1
        tmp="$(mktemp -d)"
        ( cd "$tmp" && psp-ar x "$LIB" ) || { rm -rf "$tmp"; exit 1; }
        bad=0
        for o in $(cd "$SRC" && ls *.o); do
            a="$(sha256sum "$SRC/$o" | cut -d' ' -f1)"
            b="$(sha256sum "$tmp/$o" 2>/dev/null | cut -d' ' -f1)"
            if [ "$a" != "$b" ]; then echo "DIFF $o"; bad=1; fi
        done
        rm -rf "$tmp"
        if [ "$bad" = "0" ]; then
            echo "installed pspgu matches a fresh build"
        else
            echo "installed pspgu is STALE; run $0"; exit 1
        fi
        ;;
    --depth=*)
        build "${1#--depth=}" || exit 1
        install_lib
        echo "installed pspgu with PSP_GU_DEPTH=${1#--depth=}"
        ;;
    --texfmt=*)
        PSP_GU_TEXFMT="${1#--texfmt=}"
        build || exit 1
        install_lib
        echo "installed pspgu with PSP_GU_TEXFMT=${PSP_GU_TEXFMT}"
        ;;
    *)
        build || exit 1
        install_lib
        echo "installed pspgu over $LIB"
        echo "  back up of the previous archive: $BACKUP"
        echo "  restore pspGL with: $0 --restore"
        ;;
esac
