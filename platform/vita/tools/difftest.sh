#!/bin/bash
# Differential test: runs the same scripted input on the high-address build
# (armhost, linked at 0x81000000 like the Vita) and the low-address build
# (armlow) under qemu-arm and compares every dumped frame, audio and save.
# Any difference points at code that behaves differently with Vita-style
# pointers (e.g. "ptr < 0" checks).
#
# usage: tools/difftest.sh <name> <frames> <script> [ENV=VAL ...]
#   ROM=/path/to/rom.gba  OUTDIR=/path (default ./difftest-out)
# Useful env: TMC_AUDIO_SYNC=1 (bit-exact audio), TMC_DUMP=100,110,...
#   TMC_BOOT=area,room,x,y  TMC_CHEAT_ALL=1  TMC_OVERLAY_DUMP=frame
#   TMC_QSAVE_AT=frame  TMC_QLOAD_AT=frame
set -u
name=$1; frames=$2; script=$(realpath "$3"); shift 3
B=$(cd "$(dirname "$0")/.." && pwd)
R=${ROM:?set ROM=/path/to/rom.gba}
O=${OUTDIR:-$PWD/difftest-out}
for v in hi lo; do
  d=$O/${name}_$v; rm -rf "$d"; mkdir -p "$d"; cp "$R" "$d/rom.gba"
  [ -f "$O/${name}.ini" ] && cp "$O/${name}.ini" "$d/tmcvita.ini"
  bin=$B/build-armhost/tmc_armhost; [ $v = lo ] && bin=$B/build-armlow/tmc_armlow
  ( cd "$d"; env "$@" TMC_FAST=1 GLIBC_TUNABLES=glibc.malloc.mmap_threshold=33554432 \
     qemu-arm "$bin" rom.gba "$frames" . "$script" > out.txt 2> err.txt; echo "rc=$?" >> err.txt ) &
done
wait
hi=$O/${name}_hi; lo=$O/${name}_lo; nd=0; n=0
for f in "$hi"/*.ppm; do [ -f "$f" ] || continue; b=$(basename "$f"); n=$((n+1)); cmp -s "$f" "$lo/$b" || { nd=$((nd+1)); echo "DIFF $b"; }; done
for b in audio.raw tmc_eu.sav tmc.sav; do [ -f "$hi/$b" ] || continue; cmp -s "$hi/$b" "$lo/$b" && echo "same $b" || echo "DIFF $b"; done
echo "$name: frames=$n diffs=$nd  hi:$(tail -1 "$hi/err.txt") lo:$(tail -1 "$lo/err.txt")"
