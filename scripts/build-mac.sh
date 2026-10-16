#!/usr/bin/env bash
# =====================================================================================
#  TrapWin BEND DEMON - macOS release build
#  Universal (arm64 + x86_64) Release build of VST3 + AU + Standalone, then optional
#  code signing, notarization, stapling and a .pkg installer.
#
#  Requirements: Xcode 15+ (command line tools), CMake 3.22+, git. Run on a Mac.
#
#  Environment variables (all optional - leave unset to skip that step):
#    BD_APP_SIGN_ID        "Developer ID Application: Your Name (TEAMID)"   -> codesign bundles
#    BD_INSTALLER_SIGN_ID  "Developer ID Installer: Your Name (TEAMID)"     -> sign the .pkg
#    BD_NOTARY_PROFILE     keychain profile created with:
#                            xcrun notarytool store-credentials "bend-demon-notary" \
#                              --apple-id you@example.com --team-id TEAMID --password <app-specific-pw>
#    BD_BUILD_DIR          build directory (default: build-mac)
#    BD_JUCE_DIR           use a local JUCE checkout instead of fetching it
#    BD_SKIP_TESTS=1       don't build/run the engine unit tests
#
#  Usage:   ./scripts/build-mac.sh
#  Output:  dist/mac/BEND-DEMON-<version>-mac.pkg  (+ the raw bundles in dist/mac/bundles)
# =====================================================================================
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
VERSION="$(sed -n 's/^project(BendDemon VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
BUILD_DIR="${BD_BUILD_DIR:-build-mac}"
DIST="$ROOT/dist/mac"
BUNDLES="$DIST/bundles"
ENTITLEMENTS="$ROOT/scripts/installer/mac-entitlements.plist"

echo "==> BEND DEMON $VERSION  (universal Release)"

# ---------------------------------------------------------------------------- configure
CMAKE_ARGS=(-G Xcode
            -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
            -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
            -DBEND_DEMON_BUILD_TESTS=ON)
if [[ -n "${BD_JUCE_DIR:-}" ]]; then CMAKE_ARGS+=(-DJUCE_DIR="$BD_JUCE_DIR"); fi

cmake -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"

# ---------------------------------------------------------------------------- tests
if [[ "${BD_SKIP_TESTS:-0}" != "1" ]]; then
    cmake --build "$BUILD_DIR" --config Release --target BendDemonTests -- -quiet
    "$BUILD_DIR/BendDemonTests_artefacts/Release/BendDemonTests"
fi

# ---------------------------------------------------------------------------- build
cmake --build "$BUILD_DIR" --config Release --target BendDemon_VST3 BendDemon_AU BendDemon_Standalone -- -quiet

ART="$BUILD_DIR/BendDemon_artefacts/Release"
rm -rf "$DIST"; mkdir -p "$BUNDLES"
cp -R "$ART/VST3/BEND DEMON.vst3"     "$BUNDLES/"
cp -R "$ART/AU/BEND DEMON.component"  "$BUNDLES/"
cp -R "$ART/Standalone/BEND DEMON.app" "$BUNDLES/"

for b in "$BUNDLES"/*; do
    echo "    $(basename "$b"): $(lipo -archs "$b/Contents/MacOS/"* 2>/dev/null | head -1)"
done

# ---------------------------------------------------------------------------- sign
if [[ -n "${BD_APP_SIGN_ID:-}" ]]; then
    echo "==> Signing with: $BD_APP_SIGN_ID"
    for b in "$BUNDLES/BEND DEMON.vst3" "$BUNDLES/BEND DEMON.component"; do
        codesign --force --deep --timestamp --options runtime \
                 --sign "$BD_APP_SIGN_ID" "$b"
    done
    # The standalone app hosts third-party plugins -> needs library-validation disabled.
    codesign --force --deep --timestamp --options runtime \
             --entitlements "$ENTITLEMENTS" \
             --sign "$BD_APP_SIGN_ID" "$BUNDLES/BEND DEMON.app"
    for b in "$BUNDLES"/*; do codesign --verify --strict --verbose=1 "$b"; done
else
    echo "==> BD_APP_SIGN_ID not set: skipping code signing (bundles are ad-hoc signed by Xcode)"
fi

# ---------------------------------------------------------------------------- notarize bundles
notarize () {   # $1 = file to submit (zip/pkg/dmg)
    xcrun notarytool submit "$1" --keychain-profile "$BD_NOTARY_PROFILE" --wait
}

if [[ -n "${BD_APP_SIGN_ID:-}" && -n "${BD_NOTARY_PROFILE:-}" ]]; then
    echo "==> Notarizing bundles"
    ZIP="$DIST/bundles-notarize.zip"
    ditto -c -k --keepParent "$BUNDLES" "$ZIP"
    notarize "$ZIP"
    rm -f "$ZIP"
    for b in "$BUNDLES"/*; do xcrun stapler staple "$b"; done
else
    echo "==> Notarization skipped (needs BD_APP_SIGN_ID and BD_NOTARY_PROFILE)"
fi

# ---------------------------------------------------------------------------- pkg installer
echo "==> Building installer package"
PKGROOT="$DIST/pkgroot"; PKGS="$DIST/pkgs"
rm -rf "$PKGROOT" "$PKGS"; mkdir -p "$PKGS"
mkdir -p "$PKGROOT/vst3/Library/Audio/Plug-Ins/VST3" \
         "$PKGROOT/au/Library/Audio/Plug-Ins/Components" \
         "$PKGROOT/app/Applications"
cp -R "$BUNDLES/BEND DEMON.vst3"      "$PKGROOT/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$BUNDLES/BEND DEMON.component" "$PKGROOT/au/Library/Audio/Plug-Ins/Components/"
cp -R "$BUNDLES/BEND DEMON.app"       "$PKGROOT/app/Applications/"

pkgbuild --root "$PKGROOT/vst3" --identifier com.trapwin.benddemon.vst3 --version "$VERSION" \
         --install-location / "$PKGS/BendDemon-VST3.pkg"
pkgbuild --root "$PKGROOT/au"   --identifier com.trapwin.benddemon.au   --version "$VERSION" \
         --install-location / --scripts "$ROOT/scripts/installer/mac-scripts" "$PKGS/BendDemon-AU.pkg"
pkgbuild --root "$PKGROOT/app"  --identifier com.trapwin.benddemon.app  --version "$VERSION" \
         --install-location / "$PKGS/BendDemon-App.pkg"

sed "s/@VERSION@/$VERSION/g" "$ROOT/scripts/installer/mac-distribution.xml" > "$DIST/distribution.xml"

PKG="$DIST/BEND-DEMON-$VERSION-mac.pkg"
PB_ARGS=(--distribution "$DIST/distribution.xml" --package-path "$PKGS"
         --resources "$ROOT/scripts/installer/mac-resources")
if [[ -n "${BD_INSTALLER_SIGN_ID:-}" ]]; then PB_ARGS+=(--sign "$BD_INSTALLER_SIGN_ID" --timestamp); fi
productbuild "${PB_ARGS[@]}" "$PKG"

if [[ -n "${BD_INSTALLER_SIGN_ID:-}" && -n "${BD_NOTARY_PROFILE:-}" ]]; then
    echo "==> Notarizing installer"
    notarize "$PKG"
    xcrun stapler staple "$PKG"
    spctl --assess --type install --verbose "$PKG" || true
fi

rm -rf "$PKGROOT" "$PKGS"
echo "==> Done: $PKG"
