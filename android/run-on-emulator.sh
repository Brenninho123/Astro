#!/usr/bin/env bash
# Smoke test: installs the APK on the running emulator, launches it, drives it with a few
# key events and saves screenshots. Fails if the app crashes or never renders a frame.
#
# usage: android/run-on-emulator.sh path/to/astro.apk [output-dir]
set -euo pipefail

APK="${1:?usage: run-on-emulator.sh path/to/astro.apk [output-dir]}"
OUT="${2:-build/emulator}"
PKG=org.astro.os
mkdir -p "$OUT"

fail() {
  echo "FAIL: $*"
  adb logcat -d > "$OUT/logcat-full.txt" 2>&1 || true
  adb logcat -d -s AstroOS:V AndroidRuntime:E DEBUG:V libc:F | tail -60 || true
  adb exec-out screencap -p > "$OUT/failure.png" 2>/dev/null || true
  exit 1
}

alive() {
  [ -n "$(adb shell pidof "$PKG" | tr -d '\r')" ]
}

shot() {
  adb exec-out screencap -p > "$OUT/$1.png"
  local size
  size=$(wc -c < "$OUT/$1.png")
  echo "screenshot $1.png ($size bytes)"
  [ "$size" -gt 20000 ] || fail "screenshot $1 looks empty"
}

echo "==> install"
adb install -r "$APK"
adb logcat -c

echo "==> launch"
adb shell am start -W -n "$PKG/android.app.NativeActivity"
sleep 7                     # the boot splash lasts about 2 seconds

alive || fail "the app is not running after launch"
adb logcat -d -s AstroOS:I | tee "$OUT/logcat.txt"
grep -q "Astro OS started" "$OUT/logcat.txt" || fail "native code never started"
grep -q "frame 1" "$OUT/logcat.txt" || fail "no frame was rendered"
shot 01-home

echo "==> type in the home screen: opens the launcher search"
adb shell input text "term"
sleep 2
shot 02-launcher

echo "==> Enter opens the Terminal, then run a command with the on-screen keyboard open"
adb shell input keyevent 66
sleep 2
adb shell input text "help"
adb shell input keyevent 66
sleep 1
alive || fail "the app died while using the terminal"
shot 03-terminal

echo "==> Back closes the terminal"
adb shell input keyevent 4
sleep 1
alive || fail "the app died after pressing Back"
shot 04-back-home

if adb logcat -d | grep -E "FATAL EXCEPTION|Fatal signal|SIGSEGV" | grep -i "$PKG\|astro"; then
  fail "a crash was reported in the log"
fi

adb logcat -d -s AstroOS:I > "$OUT/logcat.txt"
echo "OK: Astro OS ran on the emulator"
