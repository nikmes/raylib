#!/usr/bin/env bash
#
# Build the adopted pspGL and install it into $PSPDEV.
#
# Why this exists: the fork's 11 commits are worth 10 % (sprint 005), but a
# libGL.a that exists only on one machine is not adoptable - the next session
# could not rebuild it. This script is the recipe: pinned commit, recorded flags,
# printed checksum, install into the toolchain.
#
#   tools/pspgl-build.sh            build and install
#   tools/pspgl-build.sh --check    report whether the installed library matches
#
# Restore what pspdev ships with:
#   cp /home/psp/libGL-pspdev-shipped.a.bak $PSPDEV/psp/lib/libGL.a

set -euo pipefail

# --- pinned source -----------------------------------------------------------
# A personal fork: pin by commit, never by branch name.
PSPGL_REPO="https://github.com/TheMrIron2/pspgl.git"
PSPGL_COMMIT="ae9a6687d864c266603d38e7af9e1a64521f59e9"
# Build flags are pspGL's own Makefile defaults; only the list size is chosen.
PSPGL_PROFILE="${PSPGL_PROFILE:-0}"
# 8192 is measured, not assumed (sprint 006, N=1000 medians): 512 -> 81.24 ms,
# 1024 -> 72.37, 4096 -> 63.60, 8192 -> 61.63, 16384 -> 62.73. The curve turns
# over after 8192, so that is the adopted size. 8192 words = 32 KB per list.
PSPGL_DLIST_SIZE_WORDS="${PSPGL_DLIST_SIZE_WORDS:-8192}"

# Two objects embed __DATE__/__TIME__ in pspGL's version string ("pspEGL build
# <date>"), so the archive is never byte-identical between builds. Reproducibility
# is therefore checked on member CONTENT, excluding those two.
PSPGL_VOLATILE_MEMBERS="eglQueryString.o glGetString.o"
# sha256 over the sorted (name, sha256) of every other member. Filled in after
# the first verified build; empty means "record it, do not compare".
# Recorded for the ADOPTED configuration: fork ae9a668, PROFILE=0, DLIST_SIZE_WORDS=8192.
PSPGL_EXPECTED_CONTENT="2ab7378eafc81c5e11bcd1a17a68c8335b8da385e152927dbf273d774b4b0d2d"

usage() {
    echo "usage: $0 [--check | --verify-repeat]"
    echo "  --check          compare the installed libGL.a content against the recorded hash"
    echo "  --verify-repeat  build twice and report which members differ"
    exit 2
}

: "${PSPDEV:?PSPDEV is not set - source the pspdev environment first}"

PSPGL_SRC="${PSPGL_SRC:-/home/psp/ref/pspgl-themiriron2}"
INSTALL_LIB="$PSPDEV/psp/lib"
INSTALL_INC="$PSPDEV/psp/include"

check_mode=0
case "${1:-}" in
    "")        ;;
    --check)   check_mode=1 ;;
    --verify-repeat) check_mode=2 ;;
    *)         usage ;;
esac

installed_sha() {
    [ -f "$INSTALL_LIB/libGL.a" ] && sha256sum "$INSTALL_LIB/libGL.a" | cut -d' ' -f1 || echo "missing"
}

# Content hash: every member's own sha256, sorted, minus the timestamp-bearing ones.
content_hash() {
    local lib="$1" tmp
    tmp="$(mktemp -d)"
    ( cd "$tmp" && psp-ar x "$lib" ) || { rm -rf "$tmp"; echo "extract-failed"; return 1; }
    ( cd "$tmp" && for f in *.o; do
          case " $PSPGL_VOLATILE_MEMBERS " in *" $f "*) continue ;; esac
          sha256sum "$f"
      done | sort | sha256sum | cut -d' ' -f1 )
    rm -rf "$tmp"
}

if [ "$check_mode" = "1" ]; then
    current="$(installed_sha)"
    echo "installed libGL.a: $current"
    if [ -n "$PSPGL_EXPECTED_CONTENT" ]; then
        installed_content="$(content_hash "$INSTALL_LIB/libGL.a")"
        echo "content hash:      $installed_content"
        if [ "$installed_content" = "$PSPGL_EXPECTED_CONTENT" ]; then
            echo "matches the recorded build"
            exit 0
        fi
        echo "DOES NOT MATCH the recorded build ($PSPGL_EXPECTED_CONTENT)" >&2
        exit 1
    fi
    echo "no expected content hash recorded; nothing to compare"
    exit 0
fi

# --- source tree -------------------------------------------------------------
if [ ! -d "$PSPGL_SRC/.git" ]; then
    echo "cloning $PSPGL_REPO into $PSPGL_SRC"
    mkdir -p "$(dirname "$PSPGL_SRC")"
    git clone "$PSPGL_REPO" "$PSPGL_SRC"
fi

echo "checking out $PSPGL_COMMIT"
git -C "$PSPGL_SRC" checkout -q "$PSPGL_COMMIT"

# --- build -------------------------------------------------------------------
# The Makefile keys its objects to PROFILE / DLIST size through a config stamp,
# so a clean build is mandatory: reusing objects silently measures the previous
# configuration.
echo "building (PROFILE=$PSPGL_PROFILE, DLIST_SIZE_WORDS=$PSPGL_DLIST_SIZE_WORDS)"
make -C "$PSPGL_SRC" clean >/dev/null
make -C "$PSPGL_SRC" "PSPGL_PROFILE=$PSPGL_PROFILE" "PSPGL_DLIST_SIZE_WORDS=$PSPGL_DLIST_SIZE_WORDS"

built_sha="$(sha256sum "$PSPGL_SRC/libGL.a" | cut -d' ' -f1)"
built_content="$(content_hash "$PSPGL_SRC/libGL.a")"
echo "archive sha256: $built_sha   (changes every build: __DATE__ in two objects)"
echo "content hash:   $built_content   (what reproducibility is judged on)"

if [ -n "$PSPGL_EXPECTED_CONTENT" ] && [ "$built_content" != "$PSPGL_EXPECTED_CONTENT" ]; then
    echo "WARNING: content differs from the recorded build ($PSPGL_EXPECTED_CONTENT)" >&2
    echo "         different toolchain, flags, or the source moved" >&2
fi

if [ "$check_mode" = "2" ]; then
    echo "verifying repeatability: building a second time"
    make -C "$PSPGL_SRC" clean >/dev/null
    make -C "$PSPGL_SRC" "PSPGL_PROFILE=$PSPGL_PROFILE" "PSPGL_DLIST_SIZE_WORDS=$PSPGL_DLIST_SIZE_WORDS" >/dev/null
    second_content="$(content_hash "$PSPGL_SRC/libGL.a")"
    echo "second build content: $second_content"
    if [ "$second_content" = "$built_content" ]; then
        echo "REPRODUCIBLE except for $PSPGL_VOLATILE_MEMBERS"
        exit 0
    fi
    echo "NOT REPRODUCIBLE - content differs beyond the timestamps" >&2
    exit 1
fi

# --- install -----------------------------------------------------------------
# pspdev installs these by hand too (its package says 'make install' is unusable).
install -m 644 "$PSPGL_SRC/libGL.a"   "$INSTALL_LIB/libGL.a"
install -m 644 "$PSPGL_SRC/libGLU.a"  "$INSTALL_LIB/libGLU.a"
install -m 644 "$PSPGL_SRC/libglut.a" "$INSTALL_LIB/libglut.a"

mkdir -p "$INSTALL_INC/GL" "$INSTALL_INC/GLES"
install -m 644 "$PSPGL_SRC"/GL/*   "$INSTALL_INC/GL/"
install -m 644 "$PSPGL_SRC"/GLES/* "$INSTALL_INC/GLES/"

echo "installed into $PSPDEV/psp"
echo "relink anything that links -lGL before measuring (static link)."
