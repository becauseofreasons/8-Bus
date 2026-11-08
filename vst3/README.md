# 8-Bus (VST3) – Quelltext

Hersteller: BecauseOfReasons. Übersicht, Bedienung und Downloads: [README im Hauptordner](../README.md).

Kanalzug eines 8-Bus-Pults der 90er als VST3-Effekt: Vorverstärker mit Trim, Lo Cut, 4-Band-EQ
(Hi Mid parametrisch, Lo Mid, Hi 12k, Lo 80), Fader mit +10 dB Nachverstärkung. Die Filter sind die exakten
Schaltungen nach dem Schaltplan des Originalpults, jede Op-Amp-Stufe kann an ihren Versorgungsschienen clippen.
Bis zu 8 Kanalzüge hintereinander in einer Instanz („+“ im Fenster) – zum Verzerren von 909-Kicks über viele EQs.

Steinberg VST3 SDK + VSTGUI, ohne JUCE. Entwickelt unter Linux, Windows-Build per MinGW.


## Installieren

Fertige Plugins gibt es unter *Releases*. Nach einem eigenen Build liegen sie in `dist/` (nicht im Repository):

* **Windows:** Ordner `dist/8-Bus.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
  Der Ordner enthält 64 bit (`Contents/x86_64-win`) und 32 bit (`Contents/x86-win`); jeder Host nimmt die passende.
  32-bit-Hosts suchen unter `C:\Program Files (x86)\Common Files\VST3\`.
* **Linux:** `dist/linux/8-Bus.vst3` nach `~/.vst3/`.

## Bedienung

* Kanal-Nummer anklicken: dieser Kanal wird in der EQ-Kurve gezeigt (orange), die ganze Kette blau.
* Rechtsklick auf die Kanal-Nummer: zurücksetzen, duplizieren, entfernen. `×` entfernt, `+` fügt an.
* Regler: ziehen (Shift = fein), Doppelklick = Grundstellung, Mausrad. Beim Ziehen zeigt ein Feld den Wert.
* SETUP (Knopf unten links öffnet das Fenster): Oversampling 2×/4×/8× (Latenz immer 36 Samples),
  Bauteil-Toleranzen, Zoom 75–150 %, Presets (Menü vorbereitet), Bypass,
  Netzteil/Masse (Flattern bei heißen Kanälen: aus … ×1 wie ein echtes Pult … ×4).
* Rechts EQ-Kurve und Live-Spektrum (Ausgang grün gefüllt, Eingang als Linie, Skala dBFS rechts).
* Meter: Pult-Skala, 0 = 0 dBu = −18 dBFS. Rote LED am Kanal = OL (Übersteuerung).

## Bauen und testen

```
cmake -S vst3 -B ~/build/8bus -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ~/build/8bus
~/build/8bus/r8_curves vst3/test/ref_curves.txt          # Schaltung = Referenz
~/build/8bus/r8_enginetest [wav-ordner]                   # Klang, Latenz, Clipping, CPU
~/build/8bus/bin/Release/r8_hosttest ~/build/8bus/VST3/Release/8-Bus.vst3
~/build/8bus/bin/Release/r8_uishot <ordner>               # Oberfläche als PNG

cmake -S vst3 -B ~/build/8bus-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=vst3/cmake/mingw-w64.cmake
cmake --build ~/build/8bus-win
cmake -S vst3 -B ~/build/8bus-win32 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$PWD/vst3/cmake/mingw-w64-i686.cmake
cmake --build ~/build/8bus-win32
i686-w64-mingw32-strip -o vst3/dist/8-Bus.vst3/Contents/x86-win/8-Bus.vst3 ~/build/8bus-win32/VST3/Release/8-Bus.vst3/Contents/x86-win/8-Bus.vst3
```
`r8_uishot <ordner> [2]` schreibt auch `ui_testton.png`: rosa Rauschen + 100 Hz durch die echte Engine
(Bild in `img/`, mit `2` in doppelter Auflösung).
Referenzkurven neu erzeugen: `python3 vst3/tools/circuit_ref.py --table vst3/test/ref_curves.txt`.

## Aufbau

| Ordner | Inhalt |
|---|---|
| `src/core` | DSP ohne SDK: Schaltungslöser (`circuit`), Netzlisten der Stufen (`sections`), Kanalzug (`strip`), Oversampling, Parameter, Engine |
| `src/plugin` | VST3: Processor, Controller, Editor, Zustand |
| `src/gui` | Oberfläche (VSTGUI, alles im Code gezeichnet) |
| `tools` | Python-Referenz der Schaltung |
| `test` | Tests und Referenzdaten |
| `doc` | Bildschirmfotos, Klangbeispiele (synthetische 909-Kick) |

Details zur Schaltung und zu den Annahmen: `Analyse.md`.
