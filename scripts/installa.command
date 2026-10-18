#!/bin/bash
# Installa Synthovoks (VST3, AU, Standalone). Richiede la password di amministratore.
cd "$(dirname "$0")"
set -e
echo "Installazione di Synthovoks…"
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
# rimuove anche la versione precedente con il vecchio nome (Volks82)
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Volks82.vst3" "/Library/Audio/Plug-Ins/Components/Volks82.component" "/Applications/Volks82.app"
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Synthovoks.vst3" "/Library/Audio/Plug-Ins/Components/Synthovoks.component" "/Applications/Synthovoks.app"
sudo cp -R "Synthovoks.vst3" /Library/Audio/Plug-Ins/VST3/
sudo cp -R "Synthovoks.component" /Library/Audio/Plug-Ins/Components/
sudo cp -R "Synthovoks.app" /Applications/
sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Synthovoks.vst3" "/Library/Audio/Plug-Ins/Components/Synthovoks.component" "/Applications/Synthovoks.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Fatto. Riapri la DAW per vedere Synthovoks tra gli strumenti (Collateral Effects)."
