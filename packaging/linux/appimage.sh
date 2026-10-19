#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Wraps the packaged start program (packaging/package.py, a PyInstaller folder) in an AppImage:
#   bash packaging/linux/appimage.sh "<Title>"     ->  dist/<Title-with-dashes>-x86_64.AppImage
# The launcher keeps its data/ folder beside the .AppImage file (vw_launcher.data_dir).
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
title=${1:?usage: appimage.sh <Title>}
launcher="dist/$title PC/$title"
[[ -x $launcher ]] || { echo "Package first: python3 packaging/package.py (no $launcher)" >&2; exit 1; }
tool=out/appimagetool
if [[ ! -x $tool ]]; then
    curl -fsSL -o "$tool" https://github.com/AppImage/appimagetool/releases/download/1.9.0/appimagetool-x86_64.AppImage
    chmod +x "$tool"
fi
appdir=out/AppDir
rm -rf "$appdir"
mkdir -p "$appdir/usr/bin"
cp -r "dist/$title PC/$title" "dist/$title PC/_internal" "$appdir/usr/bin/"
cp LICENSE "$appdir/"
cp -r "dist/$title PC/licenses" "$appdir/"
cp "dist/$title PC/README.txt" "$appdir/"
cp out/icon.png "$appdir/vwport.png"
cat > "$appdir/AppRun" <<EOF
#!/bin/sh
exec "\$(dirname "\$(readlink -f "\$0")")/usr/bin/$title" "\$@"
EOF
chmod +x "$appdir/AppRun"
cat > "$appdir/vwport.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$title
Comment=Native PC port
Exec=AppRun
Icon=vwport
Categories=Game;
EOF
# CI runners have no FUSE: appimagetool runs from its extracted copy.
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 "$tool" --no-appstream "$appdir" "dist/${title// /-}-x86_64.AppImage"
echo "Built dist/${title// /-}-x86_64.AppImage"
