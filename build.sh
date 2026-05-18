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

# Create entitlements
entitlements_plist="./build/entitlements.plist"

cat > "$entitlements_plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>com.apple.security.get-task-allow</key>
  <true/>
</dict>
</plist>
EOF
[ $? = 0 ] || die "Failed to write '$entitlements_plist'"

for src in $srcs; do
  basename="${src##*/}"
  basename_wo_ext="${basename%.*}"

  out="build/$basename_wo_ext"

  # Embedding Info.plist
  bundle_id="com.pankkor.$basename_wo_ext"
  bundle_name="$basename_wo_ext"
  info_plist="./build/${basename_wo_ext}_Info.plist"
  embed_info_plist_flags="-Wl,-sectcreate,__TEXT,__info_plist,$info_plist"

  cat > "$info_plist" << EOF
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
  <key>com.apple.security.get-task-allow</key>
  <true/>
</dict>
</plist>
EOF
  [ $? = 0 ] || die "Failed to write '$info_plist'"

  # Build
  echo "Building '$src' -> '$out'..."
  set -- clang -o $out $src $cc_flags $embed_info_plist_flags
  if [ $verbose -eq 1 ]; then
    printf '%s ' "$@";
    printf '\n'
  fi
  "$@" || die "failed to build '$src'!"

  # Optional signinig
  echo "Signing  '$out'..."
  set -- codesign --force --sign 'Apple Development' --entitlements "$entitlements_plist" "$out"
  if [ $verbose -eq 1 ]; then
    printf '%s ' "$@"; printf '\n'
    printf '\n'
  fi
  "$@" || echo "failed to sign '$out'!" >&2 # Non fatal error
done

if [ $run_test -eq 1 -a -f ./build/test ]; then
  echo "Running 'build/test'..."
  ./build/test
fi
