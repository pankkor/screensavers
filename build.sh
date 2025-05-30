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

  echo "Building '$src' -> '$out'"
  echo "$build_cmd"
  echo ""

  $build_cmd || die "failed to build '$src'!"

done
