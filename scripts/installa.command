#!/bin/bash
# Installa Volks82 (VST3, AU, Standalone). Richiede la password di amministratore.
cd "$(dirname "$0")"
set -e
echo "Installazione di Volks82…"
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Volks82.vst3" "/Library/Audio/Plug-Ins/Components/Volks82.component" "/Applications/Volks82.app"
sudo cp -R "Volks82.vst3" /Library/Audio/Plug-Ins/VST3/
sudo cp -R "Volks82.component" /Library/Audio/Plug-Ins/Components/
sudo cp -R "Volks82.app" /Applications/
sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Volks82.vst3" "/Library/Audio/Plug-Ins/Components/Volks82.component" "/Applications/Volks82.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Fatto. Riapri la DAW per vedere Volks82 tra gli strumenti (Collateral Effects)."
