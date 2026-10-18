# Volks82 — emulazione del Polivoks (VST3 / AU / Standalone)

Plugin strumento in C++17 con JUCE 8, marchio **Collateral Effects**. Riproduce il sintetizzatore
duofonico sovietico Polivoks (Formanta, 1982) seguendo il manuale d'uso e gli schemi elettrici
originali (blocchi У1–У9).

![Interfaccia](docs/screenshot.png)

## Architettura (dagli schemi)

| Blocco originale | Emulazione |
|---|---|
| **У1 Tastiera** — 4 ottave Fa–Mi, 1-ГОЛ./2-ГОЛ. | Duofonia: Gen I prende la nota più bassa, Gen II la più alta. In 1 voce entrambi suonano la più bassa. Impulso di avvio a ogni cambio di tensione; la tensione resta tenuta dopo il rilascio (FET KП303). Glissando RC solo sul Gen I. |
| **У2/У3 Generatori** | Nucleo esponenziale a dente di sega; triangolo ottenuto raddrizzando la sega (ponte a diodi КД521, con la sua piccola asimmetria); impulsi dal comparatore A10 in tre larghezze. Gamme 32'…2': il Fa più basso a 32' è 21.8 Hz, il Mi più alto a 2' è 5274 Hz, come da specifiche. НАСТРОЙКА ±1 tono, РАССТРОЙКА ±2 toni (±25 %). FM esponenziale del Gen I dal Gen II. Deriva lenta (±0.5 % / 4 h nell'originale). |
| **У7 Modulatore** | Triangolo, quadra, impulsi caotici (comparatore su rumore filtrato 82k/10 nF), gradini casuali (sample & hold FET + 0.22 µF). LED di frequenza. |
| **У6 Mixer** | Sommatore invertente 22k/22k con condensatore d'uscita; ingresso esterno (ВНЕШ.). |
| **У4 Filtro** | Filtro a 2 poli con gli amplificatori programmabili K140UD12 usati come integratori: la corrente di polarizzazione fissa la frequenza e limita lo slew-rate, da cui il carattere graffiante. ФНЧ (premuto) / ПФ (rilasciato), autooscillazione verso il massimo della risonanza, taglio a zero = nessun suono (come scritto nel manuale). Pedale sul taglio. |
| **У5 Amplificatore** | VCA a coppia differenziale K118UD1. Inviluppi a 4 fasi (АТАКА, ЗАТУХАНИЕ, ПЬЕДЕСТАЛ, ПОСЛЕЗВУЧ.) per filtro e amplificatore, con modo ripetuto (∧∧). ВКЛ. tiene l'amplificatore aperto sull'ultima nota. |
| **Uscite** | ВЫХОД premuto = uscita di linea (ГРОМКОСТЬ); rilasciato = uscita cuffia (Т.ЛФ., finale KT815/KT814). СЕТЬ accende e spegne. |

Tutto il motore gira a ~176–192 kHz (sovracampionamento 4× a 44.1/48 kHz) per tenere puliti
alias e saturazioni.

## Uso con la tastiera MIDI (Arturia KeyLab)

- **Clic destro** su qualunque manopola o tasto → *MIDI Learn*, poi muovi un controllo della KeyLab.
  Le assegnazioni si salvano con il progetto.
- Assegnazioni predefinite: CC 74 taglio, CC 71 risonanza, CC 11 (pedale d'espressione) pedale
  del filtro, CC 7 volume, CC 5 glissando, CC 64 sustain (tiene i tasti).
- **Clic sul riquadro del logo**: pitch bend (non presente sull'originale, predefinito 2 semitoni),
  quantità di deriva analogica, ripristino delle assegnazioni MIDI.
- La tastiera a schermo copre le 4 ottave originali (Fa–Mi); dalla KeyLab arrivano tutte le note.
- I selettori (ДИАПАЗОН, ФОРМА, РЕЖИМ) si girano trascinando o con la rotellina.

## Compilazione

La build macOS (universale Apple Silicon + Intel) la fa GitHub Actions a ogni push su `main`:
l'archivio `Volks82-macOS` compare tra gli artefatti dell'esecuzione. Con un tag `v*`
(es. `v1.0.0`) viene creata anche una Release pubblica. Dentro c'è `Installa Volks82.command`, che
copia i plugin in `/Library/Audio/Plug-Ins/VST3` e `/Library/Audio/Plug-Ins/Components` e l'app in
`/Applicazioni`.

In locale:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Volks82_VST3 Volks82_AU Volks82_Standalone
```

Test del motore: `c++ -O2 -std=c++17 tests/engine_test.cpp -o engine_test && ./engine_test`
(intonazione, estensione 21.8 Hz–5274 Hz, autooscillazione, stabilità, CPU).

## Crediti

Font PT Sans / PT Sans Narrow (ParaType) e Russo One (Jovanny Lemonad) per il logo, tutti con SIL Open Font License (vedi `resources/fonts`).
Polivoks è un nome storico della Formanta; questo progetto non è affiliato ai produttori originali.
