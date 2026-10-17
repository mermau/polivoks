# Polivoks Study 0.1 — guida rapida (macOS)

Il progetto comprende il codice sorgente C++17 per un sintetizzatore sottrattivo con pannello frontale disegnato nel codice (ispirato all'originale), MIDI Learn e formati compilabili **VST3**, **AU** e **standalone** mediante il framework JUCE 8.0.15.

**Stato reale:** il solo motore sonoro C++ e i suoi test sono stati compilati ed eseguiti in ambiente Linux. Non sono inclusi binari AU/VST3 già pronti; non ho potuto eseguire una compilazione Xcode o verificarne il funzionamento in Ableton/Logic su macOS. Il suono non è ancora confrontato con registrazioni hardware. È un prototipo tecnico, non un'emulazione circuitale rigorosamente convalidata.

## Compilazione sul Mac

Installa Xcode (o i suoi Command Line Tools) e CMake 3.22 o successivo. Per JUCE è necessario scaricare il framework; il progetto può recuperare la versione fissa 8.0.15 durante la configurazione CMake.

Apri Terminale, entra nella cartella del progetto ed esegui:

```bash
./build-macos.sh
```

Lo script crea un progetto Xcode, tenta la build in Release di VST3, AU e dell'applicazione standalone, e lancia gli unit test. I binari si troveranno generalmente nella cartella `build-macos/PolivoksEmulation_artefacts/Release/`.

Le destinazioni tipiche per l'installazione manuale sono:

- VST3: `~/Library/Audio/Plug-Ins/VST3/`
- AU: `~/Library/Audio/Plug-Ins/Components/`

Apri il DAW e ripeti la scansione dei plugin, se necessaria. AU può essere verificato anche con `auval` su macOS. La firma del codice è un passaggio separato per la distribuzione.

## KeyLab

Carica Polivoks Study su una traccia Instrument/MIDI, invia la tastiera Arturia KeyLab alla traccia, attiva il monitoring. Per associare una manopola o uno slider, fai **clic destro su un controllo del plugin → Learn MIDI CC → muovi il controllo sulla KeyLab**. L'encoder della KeyLab deve preferibilmente trasmettere CC assoluti (0–127) e non relativi.

Gli assegnamenti restano nel preset/progetto DAW. La modulation wheel (CC1), il sustain (CC64) e il pitch bend hanno già comportamenti dedicati. Sono presenti i comandi INIT, PANIC e CLEAR MIDI CC.

## Principali punti di fedeltà ancora aperti

Il motore include doppio oscillatore, filtro LP/BP, LFO, due ADSR e duofonia condivisa. Il filtro dispone di retroazione non lineare e limiti dinamici ispirati agli operazionali programmabili originali, ma **non** implementa ancora un modello a componenti derivato integralmente dalla rete elettrica dello schema У4. Alcune calibrazioni fisiche dei potenziometri non sono disponibili nel PDF e sono scelte progettuali provvisorie.

Per raggiungere la fedeltà richiesta bisogna procedere con misure di riferimento, modello SPICE/stato non lineare, regressione audio e test effettivi AU/VST3 nei DAW. La lista tecnica dettagliata è in `docs/CIRCUIT_ANALYSIS.md`.

**Attenzione Pro Tools:** Pro Tools utilizza AAX, non carica direttamente i formati VST3 o AU prodotti. Invece Ableton Live su macOS supporta i formati richiesti.

**Licenze:** JUCE 8 è AGPLv3 o licenza commerciale. Verifica i termini prima di distribuire o commercializzare il plugin. La foto e gli schemi allegati dall'utente non sono automaticamente rilasciati per uso commerciale/redistribuzione.

## Compilazione automatica su GitHub (Apple Silicon)

Nel repository sono inclusi `.github/workflows/macos-apple-silicon.yml` e i sorgenti nella radice.

- Apri la scheda **Actions** del repository, seleziona **Build macOS Apple Silicon (VST3 + AU)** e premi **Run workflow** (ramo `main`). La compilazione parte anche dopo un push su `main`.
- La workflow usa un runner **macos-15** (Apple Silicon arm64), scarica JUCE 8.0.15 e compila **VST3**, **AU** e **Standalone** con CMake e Ninja; esegue i test DSP.
- Solo se la compilazione termina correttamente comparirà, nella pagina della run, l'artifact **Polivoks-Study-macOS-arm64**. Lo ZIP contiene le directory `package/VST3`, `package/Components` e `package/Standalone`.
- Per installare i plugin sul tuo Mac, copia i bundle `.vst3` e `.component`, rispettivamente, in `~/Library/Audio/Plug-Ins/VST3/` e `~/Library/Audio/Plug-Ins/Components/`, poi riavvia/riscansiona la DAW.

Le build sono **non firmate e non notarizzate**: Gatekeeper può richiedere ulteriori passaggi prima dell'esecuzione. La pipeline non dimostra ancora la correttezza in Ableton Live, Logic Pro o altri host. Il progetto non ha un formato AAX (Pro Tools).

**Materiali di riferimento:** la scansione PDF degli schemi elettrici non è necessaria per compilare; prima di pubblicarla in un repository pubblico è opportuno verificare il permesso di ridistribuzione. La foto in `assets/panel.jpg` non è più incorporata nel plugin (la grafica è ora disegnata nel codice) ma resta nel repository come riferimento: ha diritti da verificare per la diffusione pubblica.