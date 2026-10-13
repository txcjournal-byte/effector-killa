#!/bin/bash
# Builds an (unsigned) macOS installer: VST3 + AU into /Library/Audio/Plug-Ins
# Usage: installer/mac/build_pkg.sh <version>
set -euo pipefail
VERSION="${1:-0.2.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ART="$ROOT/build/K808_artefacts/Release"
STAGE="$ROOT/build/pkgroot"
OUT="$ROOT/build/installer"

rm -rf "$STAGE" && mkdir -p "$STAGE/Library/Audio/Plug-Ins/VST3" "$STAGE/Library/Audio/Plug-Ins/Components" "$OUT"
cp -R "$ART/VST3/808 KILLA.vst3" "$STAGE/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/808 KILLA.component" "$STAGE/Library/Audio/Plug-Ins/Components/"

pkgbuild --root "$STAGE" --identifier com.808killa.plugins --version "$VERSION" \
         --install-location / "$OUT/808KILLA-component.pkg"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>808 KILLA $VERSION</title>
    <license file="EULA.txt"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline><line choice="default"/></choices-outline>
    <choice id="default"><pkg-ref id="com.808killa.plugins"/></choice>
    <pkg-ref id="com.808killa.plugins" version="$VERSION">808KILLA-component.pkg</pkg-ref>
</installer-gui-script>
XML

mkdir -p "$OUT/resources" && cp "$ROOT/installer/EULA.txt" "$OUT/resources/"
productbuild --distribution "$OUT/distribution.xml" --resources "$OUT/resources" --package-path "$OUT" \
             "$OUT/808-KILLA-$VERSION-macOS.pkg"
rm -f "$OUT/808KILLA-component.pkg"
echo "Created $OUT/808-KILLA-$VERSION-macOS.pkg"
