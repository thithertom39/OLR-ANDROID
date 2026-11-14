#!/usr/bin/env bash
# Pinned recipe compiled and exported successfully on 2026-10-19.
# Android graphics execution still requires separate device validation.
# Run on an x86_64 Linux host/CI worker with Chromium Linux build dependencies.
# This script does not configure WSL, use sudo, install system packages, or publish.
set -euo pipefail

ANGLE_REV=e5640f4ec9ce5fec357186fd7fa40ad3030f2698
DEPOT_REV=a03dfbf7f2d1cefa2795d81d59ca4f16c8616a8f
JOBS=${JOBS:-8}
TASK_WORKSPACE=${ANGLE_WORKSPACE:-"${HOME}/oxr-angle-linux-${ANGLE_REV:0:12}"}
EXPORT_DIR=${ANGLE_EXPORT_DIR:-"${TASK_WORKSPACE}/export-arm64"}
SYNC_ONLY=0

die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
case ${1:-} in
  '') ;;
  --sync-only) SYNC_ONLY=1 ;;
  *) die 'Usage: bash angle-build-linux.sh [--sync-only]; settings via ANGLE_WORKSPACE, ANGLE_EXPORT_DIR and JOBS.' ;;
esac
[[ $# -le 1 ]] || die 'Too many arguments.'
[[ $(uname -s) == Linux ]] || die 'Android ANGLE requires a Linux build host.'
[[ $(uname -m) == x86_64 ]] || die 'This recipe targets an x86_64 Linux build host.'
[[ $JOBS =~ ^[1-9][0-9]*$ ]] || die 'JOBS must be a positive integer.'
for tool in git python3 realpath cmp find tar; do
  command -v "$tool" >/dev/null || die "Missing tool: $tool"
done

mkdir -p "$TASK_WORKSPACE"
TASK_WORKSPACE=$(realpath "$TASK_WORKSPACE")
EXPORT_DIR=$(realpath -m "$EXPORT_DIR")
DEPOT_DIR="$TASK_WORKSPACE/depot_tools"
CLIENT_DIR="$TASK_WORKSPACE/client"
ANGLE_SRC="$CLIENT_DIR/angle"
OUT_DIR="$ANGLE_SRC/out/OpenXRayArm64"

if [[ ! -d "$DEPOT_DIR/.git" ]]; then
  [[ ! -e "$DEPOT_DIR" ]] || die "Existing non-Git directory: $DEPOT_DIR"
  git init "$DEPOT_DIR"
  git -C "$DEPOT_DIR" remote add origin https://chromium.googlesource.com/chromium/tools/depot_tools.git
  git -C "$DEPOT_DIR" fetch --depth=1 origin "$DEPOT_REV"
  git -C "$DEPOT_DIR" checkout --detach FETCH_HEAD
fi
[[ $(git -C "$DEPOT_DIR" rev-parse HEAD) == "$DEPOT_REV" ]] || die 'depot_tools checkout has a different revision; choose a new workspace.'
[[ -z $(git -C "$DEPOT_DIR" status --porcelain --untracked-files=no) ]] || die 'Tracked depot_tools files have local changes.'
export PATH="$DEPOT_DIR:$PATH"
export DEPOT_TOOLS_UPDATE=0
export PYTHONDONTWRITEBYTECODE=1

# The pinned autoninja wrapper needs depot_tools' hermetic Python marker.
# gclient's vpython path alone does not initialize it when updates are disabled.
# This supported bootstrap keeps the pinned depot_tools Git revision unchanged.
if [[ ! -s "$DEPOT_DIR/python3_bin_reldir.txt" ]]; then
  "$DEPOT_DIR/ensure_bootstrap"
fi

mkdir -p "$CLIENT_DIR"
EXPECTED_CONFIG="$CLIENT_DIR/.gclient.openxray-expected"
cat > "$EXPECTED_CONFIG" <<EOF
solutions = [{
  'name': 'angle',
  'url': 'https://chromium.googlesource.com/angle/angle.git',
  'deps_file': 'DEPS',
  'managed': False,
  'custom_deps': {},
  'custom_vars': {
    'checkout_angle_cl_deps': False,
    'checkout_angle_dawn_deps': False,
    'checkout_angle_mesa': False,
    'checkout_angle_restricted_traces': False,
    'checkout_extra_traces': False,
  },
}]
target_os = ['android']
EOF
if [[ -f "$CLIENT_DIR/.gclient" ]]; then
  cmp -s "$EXPECTED_CONFIG" "$CLIENT_DIR/.gclient" || die 'Existing .gclient differs from this recipe; choose a new workspace.'
else
  cp "$EXPECTED_CONFIG" "$CLIENT_DIR/.gclient"
fi

# gclient obtains the Linux SDK/NDK, Chromium clang, GN and dependencies pinned
# by ANGLE DEPS. Do not point this build at the Windows NDK used by OpenXRay.
cd "$CLIENT_DIR"
gclient sync --revision "angle@$ANGLE_REV" --shallow --no-history --jobs "$JOBS"
[[ $(git -C "$ANGLE_SRC" rev-parse HEAD) == "$ANGLE_REV" ]] || die 'Unexpected ANGLE revision after sync.'
if [[ $SYNC_ONLY == 1 ]]; then
  printf 'Source ready. Install Linux dependencies separately if needed:\n  cd %q\n  sudo ./build/install-build-deps.sh --android\nThen rerun this script without --sync-only.\n' "$ANGLE_SRC"
  exit 0
fi

for tool in readelf sha256sum; do
  command -v "$tool" >/dev/null || die "Missing tool: $tool (binutils/coreutils)"
done
[[ ! -e "$EXPORT_DIR" || -d "$EXPORT_DIR" ]] || die 'Export target is not a directory.'
mkdir -p "$EXPORT_DIR"
[[ -z $(find "$EXPORT_DIR" -mindepth 1 -maxdepth 1 -print -quit) ]] || die 'Export directory is not empty; choose a new ANGLE_EXPORT_DIR.'

cd "$ANGLE_SRC"
GN="$ANGLE_SRC/buildtools/linux64/gn"
[[ -x "$GN" ]] || die 'gclient did not provision Linux GN.'
mkdir -p "$OUT_DIR"
cat > "$OUT_DIR/args.gn" <<'EOF'
target_os = "android"
target_cpu = "arm64"
arm_control_flow_integrity = "none"
is_component_build = false
is_debug = false
is_clang = true
angle_enable_vulkan = true
angle_enable_gl = true
angle_enable_null = false
angle_enable_trace = false
angle_enable_vulkan_validation_layers = false
use_remoteexec = false
use_siso = false
symbol_level = 1
EOF
"$GN" gen "$OUT_DIR" --fail-on-unused-args
autoninja -C "$OUT_DIR" -j "$JOBS" libEGL libGLESv2

LIB_DIR="$EXPORT_DIR/arm64-v8a"
mkdir -p "$LIB_DIR"
printf 'library\tneeded\tprovider\n' > "$EXPORT_DIR/runtime-dependencies.tsv"
declare -A seen=()
queue=(libEGL_angle.so libGLESv2_angle.so)
for ((index=0; index<${#queue[@]}; index++)); do
  library=${queue[index]}
  [[ -z ${seen[$library]+present} ]] || continue
  seen[$library]=1
  library_path="$OUT_DIR/$library"
  [[ -f "$library_path" ]] || die "Runtime library missing from build output: $library"
  readelf -h "$library_path" | grep -q 'Machine:.*AArch64' || die "Unexpected ABI: $library"
  cp "$library_path" "$LIB_DIR/$library"
  readelf -h -l -d "$library_path" > "$EXPORT_DIR/$library.elf.txt"
  while IFS= read -r needed; do
    case "$needed" in
      libc.so|libm.so|libdl.so|liblog.so|libandroid.so|libnativewindow.so|libvulkan.so|libEGL.so|libGLESv1_CM.so|libGLESv2.so|libz.so)
        provider=android-system
        ;;
      *)
        provider=bundled
        [[ -f "$OUT_DIR/$needed" ]] || die "$library needs $needed; it must be located and packaged before use."
        queue+=("$needed")
        ;;
    esac
    printf '%s\t%s\t%s\n' "$library" "$needed" "$provider" >> "$EXPORT_DIR/runtime-dependencies.tsv"
  done < <(readelf -d "$library_path" | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p')
done

cp "$OUT_DIR/args.gn" "$EXPORT_DIR/args.gn"
cp "$ANGLE_SRC/DEPS" "$EXPORT_DIR/ANGLE-DEPS.txt"
printf 'ANGLE=%s\nDEPOT_TOOLS=%s\nHOST=%s\n' "$ANGLE_REV" "$DEPOT_REV" "$(uname -a)" > "$EXPORT_DIR/build-revisions.txt"
cd "$CLIENT_DIR"
# revinfo resolves CIPD tags through the network even with --actual. Record
# installed immutable package instance IDs directly from the local CIPD state.
gclient revinfo --actual --ignore-dep-type=cipd --output-json="$EXPORT_DIR/dependency-revisions.json" > "$EXPORT_DIR/dependency-revisions.txt"
"$DEPOT_DIR/.cipd_client" installed -root "$CLIENT_DIR" -json-output "$EXPORT_DIR/cipd-installed.json" > "$EXPORT_DIR/cipd-installed.txt"

# Preserve license and attribution candidates from the entire resolved source
# tree, with relative paths. Final redistribution still requires a license review.
cd "$ANGLE_SRC"
find . -type d \( -name .git -o -path ./out \) -prune -o -type f \
  \( -iname '*license*' -o -iname '*copying*' -o -iname 'notice*' -o -iname 'unlicense*' -o -name README.chromium \) \
  -print0 > "$EXPORT_DIR/license-files.list0"
tar --null -T "$EXPORT_DIR/license-files.list0" -czf "$EXPORT_DIR/source-license-notices.tar.gz"
cp LICENSE "$EXPORT_DIR/ANGLE-LICENSE.txt"
cd "$LIB_DIR"
sha256sum ./*.so > "$EXPORT_DIR/SHA256SUMS.txt"
printf 'Export ready: %s\nCheck DT_NEEDED and licenses, then copy arm64-v8a/*.so into the Android project jniLibs/arm64-v8a.\n' "$EXPORT_DIR"
