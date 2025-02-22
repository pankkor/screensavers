#!/bin/sh

die() {
  echo 'Error: ' "$@" >&2
  exit 1
}

mkdir -p build || die "failed to make 'build' directory!"

cc_flags="$(cat compile_flags.txt)"

for src in src/*.c; do
  basename="${src##*/}"
  basename_wo_ext="${basename%.*}"

  out="build/$basename_wo_ext"

  # Build macOS aarch64 OpenGL
  build_cmd="clang -o $out $src $cc_flags"

  echo "Building '$src' -> '$out'"
  echo "$build_cmd"
  echo ""

  $build_cmd || die "failed to build '$src'!"
done
