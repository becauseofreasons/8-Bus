# 8-Bus

**Kanalzug-Emulation eines analogen 8-Bus-Pults der 90er als VST3-Plugin – bis zu 8 Kanäle in Kette.**
**Channel-strip emulation of a 90s analog 8-bus console as a VST3 plugin – up to 8 channels in series.**

[Deutsch](#deutsch) · [English](#english)

<img src="img/8-Bus@2x.png" alt="8-Bus mit Testton (rosa Rauschen + 100 Hz über 4 Kanäle) / 8-Bus with a test tone (pink noise + 100 Hz through 4 channels)" width="100%">

*Testton (rosa Rauschen + 100-Hz-Sinus) durch 4 Kanäle: Eingang blau, Ausgang grün, EQ-Kurven von Kanal 2 (orange) und der ganzen Kette (hellblau).*
*Test tone (pink noise + 100 Hz sine) through 4 channels: input blue, output green, EQ curves of channel 2 (orange) and of the whole chain (light blue).*

| | |
|---|---|
| Version | 1.0.0 |
| Hersteller / Vendor | BecauseOfReasons |
| Format | VST3 (Effekt, Kategorie EQ / effect, category EQ) |
| Plattformen / Platforms | Windows 64 bit + 32 bit, Linux x86_64 |
| Download | [Releases](https://github.com/becauseofreasons/8-Bus/releases/latest) |

---

## Deutsch

### Worum es geht

In den 90ern haben Techno-Produzenten ihre 909-Kicks über viele Kanäle eines Mischpults hintereinander gejagt:
Trim aufgedreht, Bässe angehoben, Fader hoch – und das Signal in den nächsten Kanal. Jeder Kanal verzerrt ein
bisschen mehr, die Kick wird dichter, und wenn das Pult heiß gefahren wird, fängt es an zu „flattern“: Netzteil
und Masse geben unter der Last nach, und die Kick bekommt einen langen, tiefen Ausklang.

**8-Bus** bildet genau das nach. Es ist kein Nachbau „nach Gehör“, sondern die Schaltung des Kanalzugs –
Vorverstärker, Lo Cut, EQ, Fader-Stufe – Bauteil für Bauteil gerechnet. Bis zu acht dieser Kanäle laufen in
einer Instanz hintereinander, jeder mit eigenen Reglern.

Das Projekt ist unabhängig und steht in keiner Verbindung zu einem Hersteller von Mischpulten.

### Funktionen

* **Kanalzug wie im Original**, je Kanal:
  * **Trim** (−5 … +40 dB) mit dem Vorverstärker: Transistor-Differenzstufe mit stromgrenztem Koppelkondensator,
    dadurch der typische „Pump“ bei hartem Anfahren.
  * **Lo Cut** 18 dB/Oktave (−3 dB bei 85 Hz).
  * **Hi Mid** parametrisch: ±15 dB, 500 Hz … 18 kHz, Bandbreite 3 … 1/12 Oktave.
  * **Lo Mid**: ±15 dB, 45 Hz … 3 kHz.
  * **Hi** (Kuhschwanz 12 kHz) und **Lo** (Kuhschwanz 80 Hz), je ±15 dB.
  * **EQ** ein/aus, **Fader** (∞ … +10 dB) mit Nachverstärkung.
  * **OL-LED** wie am Pult.
* **Echte Schaltungssimulation:** Jede Filterstufe ist die Netzliste der Originalschaltung (Knotenanalyse,
  ideale Operationsverstärker) und wird pro Sample gelöst. Die Kurven stimmen mit einem unabhängigen
  Referenzmodell auf 0,00001 dB überein.
* **Übersteuerung wie analog:** Jeder Operationsverstärker kann an seinen Versorgungsschienen (rund ±15 V) clippen, mit weichem
  Knie. Wo und wie stark es zerrt, hängt also davon ab, *welche* Stufe übersteuert.
* **Bis zu 8 Kanäle in Kette** in einer Instanz – `+` fügt einen an, `×` entfernt ihn.
* **„Flattern“ über Netzteil und Masse:** Bei heiß gefahrenen Kanälen sackt die Versorgung ab und die Masse
  schwingt (um 52 Hz) – daraus entsteht der lange Kick-Ausklang. Regelbar von *aus* über *×1* (typisches Pult)
  bis *×4*. Das Pult schwingt dabei nie von selbst; ohne Eingangssignal klingt alles in ~1 s aus.
* **Bauteil-Toleranzen** (zuschaltbar): Jede Instanz bekommt eigene, leicht abweichende Bauteilwerte – wie zwei
  Kanäle eines echten Pults, die nie ganz gleich klingen.
* **Oversampling** 2× / 4× / 8× (Standard 4×), Latenz immer 36 Samples; wird dem Host gemeldet.
* **Live-Analyzer** im Stil moderner EQs: Eingang und Ausgang, zwei FFT-Auflösungen (genaue Tonhöhe im Bass,
  schnelle Höhen), weiche Kurven, Spitzenwertlinie, Neigung 4,5 dB/Oktave, ~60 Bilder pro Sekunde.
* **EQ-Kurve** des gewählten Kanals und der ganzen Kette.
* **Pegel wie am Pult:** −18 dBFS entsprechen 0 dBu. LED-Meter für Eingang und Ausgang mit Pult-Skala.
* **Alles automatisierbar**, Zustand wird mit dem Projekt gespeichert.
* **Zoom** 75 / 100 / 125 / 150 %.

### Installation

1. Unter [Releases](https://github.com/becauseofreasons/8-Bus/releases/latest) die passende ZIP-Datei laden.
2. Entpacken. Der Ordner `8-Bus.vst3` ist das Plugin.
3. Ordner kopieren:
   * **Windows:** nach `C:\Program Files\Common Files\VST3\`. Der Ordner enthält die 64- und die 32-bit-Version;
     jeder Host nimmt die passende. 32-bit-Hosts suchen unter `C:\Program Files (x86)\Common Files\VST3\`.
   * **Linux:** nach `~/.vst3/`.
4. In der DAW nach Plugins suchen lassen (z. B. Ableton: *Einstellungen → Plug-Ins → VST3-Ordner neu scannen*).
   Das Plugin heißt **8-Bus**, Hersteller **BecauseOfReasons**.

macOS wird nicht unterstützt.

### Bedienung

* **Kanalnummer anklicken:** Dieser Kanal wird in der EQ-Kurve gezeigt (orange), die ganze Kette hellblau.
* **Rechtsklick auf die Kanalnummer:** zurücksetzen, duplizieren, entfernen.
* **Regler:** ziehen (mit Shift fein), Doppelklick = Grundstellung, Mausrad. Beim Ziehen zeigt ein Feld den Wert.
* **IN / OUT:** Eingangs- und Ausgangspegel ±24 dB.
* **SETUP** (Knopf unten links) öffnet die Einstellungen:

| Einstellung | Wirkung |
|---|---|
| Oversampling | 2× / 4× / 8× – mehr = sauberere Verzerrung, mehr Rechenzeit |
| Toleranzen | Bauteil-Streuung aus/an |
| Zoom % | Fenstergröße |
| Presets | Menü (Werks-Presets folgen) |
| Bypass | Signal unbearbeitet, latenzgleich |
| Netzteil/Masse | Stärke des Flatterns: aus … ×1 … ×4 |
| Analyzer: Anzeige | aus / Eingang + Ausgang / nur Ausgang |
| Analyzer: Tempo | langsam / mittel / schnell |
| Analyzer: Bereich | 60 / 90 / 120 dB |
| Analyzer: Neigung | 0 oder 4,5 dB/Oktave |
| Analyzer: Spitzenwert | Spitzenwertlinie aus/an |

**Tipp für die Kick-Kette:** 4–6 Kanäle, Trim im ersten Kanal deutlich auf, Lo +6 … +12 dB, Fader über U –
dann den Trim eines zweiten Kanals dazunehmen und mit *Netzteil/Masse* die Länge des Ausklangs einstellen.

### Rechenzeit

Gemessen auf einem Kern (10 s Stereo, 48 kHz, 4× Oversampling): 1 Kanal ≈ 6 %, 8 Kanäle ≈ 28 % eines Kerns.

### Aus dem Quelltext bauen

Steinberg VST3 SDK 3.8 und VSTGUI, ohne JUCE. Entwickelt unter Linux, Windows-Builds per MinGW (Cross-Compiler).
Befehle, Tests und Aufbau des Quelltexts: [vst3/README.md](vst3/README.md).
Wie die Schaltung modelliert ist und welche Annahmen gelten: [vst3/Analyse.md](vst3/Analyse.md).

### Rechtliches

© 2026 BecauseOfReasons. Alle Rechte vorbehalten.
VST ist eine Marke der Steinberg Media Technologies GmbH.

---

## English

### What it is

In the 90s, techno producers ran their 909 kicks through many channels of a mixing console in a row: trim up,
bass boosted, fader up – and into the next channel. Every channel adds a little more drive, the kick gets denser,
and when the console is pushed hard it starts to "flutter": supply and ground sag under the load, and the kick
gets a long, deep tail.

**8-Bus** recreates exactly that. It is not modelled "by ear" but from the circuit of the channel strip –
preamp, low cut, EQ, fader stage – computed component by component. Up to eight of these channels run in series
inside one instance, each with its own controls.

This is an independent project with no affiliation to any console manufacturer.

### Features

* **Channel strip like the original**, per channel:
  * **Trim** (−5 … +40 dB) with the preamp: transistor differential stage with a current-limited coupling
    capacitor, which gives the typical "pump" when driven hard.
  * **Low cut** 18 dB/octave (−3 dB at 85 Hz).
  * **Hi mid** parametric: ±15 dB, 500 Hz … 18 kHz, bandwidth 3 … 1/12 octave.
  * **Lo mid**: ±15 dB, 45 Hz … 3 kHz.
  * **Hi** (12 kHz shelf) and **Lo** (80 Hz shelf), ±15 dB each.
  * **EQ** in/out, **fader** (∞ … +10 dB) with make-up gain.
  * **OL LED** like on the console.
* **Real circuit simulation:** every filter stage is the netlist of the original circuit (nodal analysis, ideal
  op-amps), solved per sample. The curves match an independent reference model to within 0.00001 dB.
* **Analog-style overload:** every op-amp can clip at its supply rails (around ±15 V) with a soft knee, so where and how it
  distorts depends on *which* stage is driven.
* **Up to 8 channels in series** in one instance – `+` adds one, `×` removes it.
* **"Flutter" via supply and ground:** with hot channels the supply sags and the ground rings (around 52 Hz) –
  this creates the long kick tail. Adjustable from *off* via *×1* (typical console) to *×4*. The console never
  oscillates on its own; without input everything dies away within ~1 s.
* **Component tolerances** (optional): each instance gets its own slightly different component values – like two
  channels of a real console that never sound quite the same.
* **Oversampling** 2× / 4× / 8× (default 4×), latency always 36 samples, reported to the host.
* **Live analyzer** in the style of modern EQs: input and output, two FFT resolutions (exact pitch in the bass,
  fast highs), smooth curves, peak line, 4.5 dB/octave tilt, ~60 frames per second.
* **EQ curve** of the selected channel and of the whole chain.
* **Console levels:** −18 dBFS equals 0 dBu. LED meters for input and output with the console scale.
* **Everything automatable**, state is saved with the project.
* **Zoom** 75 / 100 / 125 / 150 %.

### Installation

1. Download the matching ZIP file from [Releases](https://github.com/becauseofreasons/8-Bus/releases/latest).
2. Unzip it. The folder `8-Bus.vst3` is the plugin.
3. Copy the folder:
   * **Windows:** to `C:\Program Files\Common Files\VST3\`. The folder contains the 64-bit and the 32-bit version;
     each host picks the right one. 32-bit hosts look in `C:\Program Files (x86)\Common Files\VST3\`.
   * **Linux:** to `~/.vst3/`.
4. Let your DAW rescan its plugins (e.g. Ableton: *Settings → Plug-Ins → rescan VST3 folders*).
   The plugin is called **8-Bus**, vendor **BecauseOfReasons**.

macOS is not supported.

### Usage

The user interface is in German. Main terms: *Kanal* = channel, *Kette* = chain, *Eingang/Ausgang* =
input/output, *Netzteil/Masse* = supply/ground, *Flattern* = flutter, *aus/an* = off/on.

* **Click a channel number:** that channel is shown in the EQ curve (orange), the whole chain in light blue.
* **Right-click a channel number:** reset, duplicate, remove.
* **Knobs:** drag (Shift = fine), double-click = default, mouse wheel. While dragging, a bubble shows the value.
* **IN / OUT:** input and output level ±24 dB.
* **SETUP** (button bottom left) opens the settings:

| Setting | Effect |
|---|---|
| Oversampling | 2× / 4× / 8× – more = cleaner distortion, more CPU |
| Toleranzen (tolerances) | component spread off/on |
| Zoom % | window size |
| Presets | menu (factory presets will follow) |
| Bypass | unprocessed signal, same latency |
| Netzteil/Masse (supply/ground) | amount of flutter: off … ×1 … ×4 |
| Analyzer: Anzeige (display) | off / input + output / output only |
| Analyzer: Tempo (speed) | slow / medium / fast |
| Analyzer: Bereich (range) | 60 / 90 / 120 dB |
| Analyzer: Neigung (tilt) | 0 or 4.5 dB/octave |
| Analyzer: Spitzenwert (peak) | peak line off/on |

**Tip for the kick chain:** 4–6 channels, trim of the first channel well up, Lo +6 … +12 dB, faders above U –
then bring up the trim of a second channel and set the length of the tail with *Netzteil/Masse*.

### CPU load

Measured on one core (10 s stereo, 48 kHz, 4× oversampling): 1 channel ≈ 6 %, 8 channels ≈ 28 % of a core.

### Building from source

Steinberg VST3 SDK 3.8 and VSTGUI, no JUCE. Developed on Linux, Windows builds via MinGW (cross compiler).
Commands, tests and source layout: [vst3/README.md](vst3/README.md) (German).
How the circuit is modelled and which assumptions apply: [vst3/Analyse.md](vst3/Analyse.md) (German).

### Legal

© 2026 BecauseOfReasons. All rights reserved.
VST is a trademark of Steinberg Media Technologies GmbH.
