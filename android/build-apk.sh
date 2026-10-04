#!/usr/bin/env bash
# Builds the Astro OS Android app (build/astro.apk) with the plain Android SDK and NDK:
# no Gradle and no Java code - clang compiles libastro.so, aapt2 packs it, apksigner signs it.
#
# Works on Linux (GitHub Actions) and on Windows through Git Bash.
# Environment: ANDROID_SDK_ROOT / ANDROID_HOME (or the default Windows SDK location),
#              ANDROID_NDK_HOME (optional, the newest NDK in the SDK is used otherwise),
#              ABIS (optional, e.g. ABIS=x86_64 for a quick emulator-only build).
set -euo pipefail
cd "$(dirname "$0")/.."

MINSDK=26
ABIS="${ABIS:-arm64-v8a armeabi-v7a x86_64}"

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) HOST=windows-x86_64; EXE=.exe; BAT=.bat ;;
  Darwin)               HOST=darwin-x86_64;  EXE=;     BAT= ;;
  *)                    HOST=linux-x86_64;   EXE=;     BAT= ;;
esac

SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
if [ -z "$SDK" ] && [ -n "${LOCALAPPDATA:-}" ]; then
  SDK="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk"
fi
[ -d "$SDK" ] || { echo "Android SDK not found (set ANDROID_SDK_ROOT)"; exit 1; }

NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_ROOT:-}}}"
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
  NDK="$(ls -d "$SDK"/ndk/* | sort -V | tail -1)"
fi
BT="$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)"
PLATFORM="$(ls -d "$SDK"/platforms/android-[0-9][0-9] | sort -V | tail -1)"
API="${PLATFORM##*-}"
CLANG="$NDK/toolchains/llvm/prebuilt/$HOST/bin/clang$EXE"
GLUE="$NDK/sources/android/native_app_glue"
VERSION="$(sed -n 's/^#define ASTRO_VERSION "\(.*\)"/\1/p' src/kernel/version.h)"

echo "SDK:        $SDK"
echo "NDK:        $NDK"
echo "Build tools: $BT"
echo "Platform:   android-$API  (minSdk $MINSDK)"
echo "Version:    $VERSION"

OUT=build/android
rm -rf "$OUT/pkg" "$OUT/res.zip" "$OUT"/astro-*.apk build/astro.apk
mkdir -p "$OUT"

# ---- native library, one per ABI ----
SOURCES=( $(ls src/kernel/*.c | grep -v kstring.c) src/desktop/*.c src/android/*.c "$GLUE/android_native_app_glue.c" )

for abi in $ABIS; do
  case "$abi" in
    arm64-v8a)   triple=aarch64-linux-android ;;
    armeabi-v7a) triple=armv7a-linux-androideabi ;;
    x86_64)      triple=x86_64-linux-android ;;
    x86)         triple=i686-linux-android ;;
    *) echo "unknown ABI $abi"; exit 1 ;;
  esac
  echo "==> libastro.so ($abi)"
  mkdir -p "$OUT/pkg/lib/$abi"
  "$CLANG" --target="${triple}${MINSDK}" -std=gnu11 -O2 -fPIC -Wall -Wextra -Wno-unused-parameter \
    -DASTRO_HOSTED -DASTRO_ANDROID -Isrc/kernel -Isrc/desktop -I"$GLUE" \
    "${SOURCES[@]}" -shared -o "$OUT/pkg/lib/$abi/libastro.so" \
    -llog -landroid -lm -Wl,-u,ANativeActivity_onCreate -Wl,--no-undefined -Wl,-z,max-page-size=16384
done

# ---- package ----
echo "==> packaging"
"$BT/aapt2$EXE" compile --dir android/res -o "$OUT/res.zip"
"$BT/aapt2$EXE" link -o "$OUT/astro-unsigned.apk" -I "$PLATFORM/android.jar" \
  --manifest android/AndroidManifest.xml \
  --min-sdk-version "$MINSDK" --target-sdk-version "$API" \
  --version-code 1 --version-name "$VERSION" "$OUT/res.zip"

(
  cd "$OUT/pkg"
  for lib in $(find lib -name '*.so' | sort); do
    "$BT/aapt$EXE" add ../astro-unsigned.apk "$lib" > /dev/null
  done
)

"$BT/zipalign$EXE" -f -p 4 "$OUT/astro-unsigned.apk" "$OUT/astro-aligned.apk"

# ---- sign with a throwaway debug key (this is a test build, not a store release) ----
if [ ! -f "$OUT/debug.keystore" ]; then
  keytool -genkeypair -keystore "$OUT/debug.keystore" -storepass android -keypass android \
    -alias astro -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Astro Debug, O=Astro, C=US" > /dev/null 2>&1
fi
"$BT/apksigner$BAT" sign --ks "$OUT/debug.keystore" --ks-pass pass:android --key-pass pass:android \
  --out build/astro.apk "$OUT/astro-aligned.apk"
"$BT/apksigner$BAT" verify build/astro.apk

echo "Built build/astro.apk ($(wc -c < build/astro.apk) bytes)"
