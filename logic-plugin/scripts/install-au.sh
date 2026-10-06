#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
plugin_build="${1:-$project_dir/build}"
component="$plugin_build/CSynth_artefacts/Release/AU/CSynth.component"
if [[ ! -d "$component" ]]; then
    component="$plugin_build/CSynth_artefacts/AU/CSynth.component"
fi
if [[ ! -d "$component" ]]; then
    echo "Couldn't find CSynth.component in $plugin_build. Build CSynth_AU in Release first." >&2
    exit 1
fi
plugin_folder="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$plugin_folder"
if [[ -e "$plugin_folder/CSynth.component" ]]; then
    backup_folder="$HOME/Library/Audio/CSynth-backups"
    mkdir -p "$backup_folder"
    backup_path="$backup_folder/CSynth-$(date +%Y%m%d-%H%M%S)-$$.component"
    mv "$plugin_folder/CSynth.component" "$backup_path"
    echo "Previous build saved at $backup_path"
fi
ditto "$component" "$plugin_folder/CSynth.component"
codesign --verify --deep --strict "$plugin_folder/CSynth.component"
echo "Installed $plugin_folder/CSynth.component"
echo "Reopen Logic, or use Plug-In Manager > Reset & Rescan Selection."
