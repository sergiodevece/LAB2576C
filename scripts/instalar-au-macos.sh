#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Darwin ]]; then
    echo "Este instalador del Audio Unit se ejecuta en macOS." >&2
    exit 1
fi

if ! command -v cmake >/dev/null || ! command -v ctest >/dev/null; then
    echo "Hace falta CMake 3.22 o superior en el PATH." >&2
    exit 1
fi

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$project_dir/build-au"
build_options=(
    -DCMAKE_BUILD_TYPE=Release
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
    -DBUILD_TESTING=ON
)

if [[ $# -gt 0 ]]; then
    build_options+=("-DLAB2576C_JUCE_PATH=$1")
fi

xcrun --sdk macosx --show-sdk-path >/dev/null
cmake -S "$project_dir" -B "$build_dir" "${build_options[@]}"
cmake --build "$build_dir" --config Release --parallel 4
ctest --test-dir "$build_dir" -C Release --output-on-failure

product="$build_dir/LAB2576C_artefacts/Release/AU/LAB2576C.component"
if [[ ! -d "$product" ]]; then
    echo "El Audio Unit no aparece en $product" >&2
    exit 1
fi
/usr/bin/lipo "$product/Contents/MacOS/LAB2576C" -verify_arch arm64 x86_64

plugin_dir="$HOME/Library/Audio/Plug-Ins/Components"
installed="$plugin_dir/LAB2576C.component"
mkdir -p "$plugin_dir"
stage="$(mktemp -d "$plugin_dir/.LAB2576C.XXXXXXXX")"
trap 'rm -rf -- "$stage"' EXIT

staged="$stage/LAB2576C.component"
/usr/bin/ditto "$product" "$staged"
/usr/bin/codesign --force --sign - "$staged"
/usr/bin/codesign --verify "$staged"

backup=""
if [[ -e "$installed" || -L "$installed" ]]; then
    backup_dir="$(mktemp -d "$build_dir/previous-au.XXXXXXXX")"
    backup="$backup_dir/LAB2576C.component"
    mv "$installed" "$backup"
fi

if ! mv "$staged" "$installed"; then
    if [[ -n "$backup" ]]; then
        mv "$backup" "$installed"
    fi
    exit 1
fi

echo "Instalado en $installed"
if [[ -n "$backup" ]]; then
    echo "Versión anterior guardada en $backup"
fi

info_plist="$installed/Contents/Info.plist"
au_type="$(/usr/libexec/PlistBuddy -c 'Print :AudioComponents:0:type' "$info_plist")"
au_subtype="$(/usr/libexec/PlistBuddy -c 'Print :AudioComponents:0:subtype' "$info_plist")"
au_manufacturer="$(/usr/libexec/PlistBuddy -c 'Print :AudioComponents:0:manufacturer' "$info_plist")"

if ! /usr/bin/auval -a | /usr/bin/awk -v type="$au_type" -v subtype="$au_subtype" -v manufacturer="$au_manufacturer" '
    $1 == type && $2 == subtype && $3 == manufacturer { found = 1 }
    END { exit !found }
'; then
    echo "Actualizando el registro de Audio Units para $au_type $au_subtype $au_manufacturer"
    /usr/bin/killall -u "$(/usr/bin/id -un)" AudioComponentRegistrar 2>/dev/null || true
    /bin/sleep 1
fi

/usr/bin/auval -v "$au_type" "$au_subtype" "$au_manufacturer"
