#!/bin/sh

cc_flags="$(cat compile_flags.txt)"

help="
Build script.

Usage
  build.sh [options] [source...]

Creates 'build' directory and builds only specified source paths.
If no source paths are specified builds all targers in 'src/*.c' with
build flags taken from 'compiler_flags.txt'.
Build also embeds Info.plist into binaries. No signing is done so far.

Options
  --help,-h         This help.
  --verbose,-v      Verbose build. Print build command.
  --no-test         Don't run test at '/build/test'.
"

die() {
  echo 'Error: ' "$@" >&2
  exit 1
}

verbose=0
run_test=1
srcs='src/*.c'

while [ $# -gt 0 ]; do
  case "$1" in
    --help|-h)
      echo "$help"
      exit 0
      ;;
    --verbose|-v)
      verbose=1
      shift
      ;;
    --no-test)
      run_test=0
      shift
      ;;
    -*)
      echo "Error: unknown option '$1'"
      echo "$help"
      exit 1
      ;;
    *)
      srcs="$@"
      shift $#
      ;;
  esac
done

mkdir -p build || die "failed to make 'build' directory!"

for src in $srcs; do
  basename="${src##*/}"
  basename_wo_ext="${basename%.*}"

  out="build/$basename_wo_ext"

  # Embedding Info.plist
  bundle_id="com.pankkor.$basename_wo_ext"
  bundle_name="$basename_wo_ext"
  info_plist="./build/${basename_wo_ext}_Info.plist"
  embed_info_plist_flags="-Wl,-sectcreate,__TEXT,__info_plist,$info_plist"

  cat > build/${basename_wo_ext}_Info.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key>
  <string>${bundle_id}</string>
  <key>CFBundleName</key>
  <string>${bundle_name}</string>
  <key>CFBundleVersion</key>
  <string>1.0</string>
  <key>MetalCaptureEnabled</key>
  <true/>
</dict>
EOF

  [ $? = 0 ] || die "Failed to write '$info_plist'"

  # Build macOS aarch64 OpenGL
  build_cmd="clang -o $out $src $embed_info_plist_flags $cc_flags"

  echo "Building '$src' -> '$out'..."
  if [ $verbose -eq 1 ]; then
    echo "$build_cmd"
    echo ""
  fi

  $build_cmd || die "failed to build '$src'!"
done

if [ $run_test -eq 1 -a -f ./build/test ]; then
  echo "Running 'build/test'..."
  ./build/test
fi
