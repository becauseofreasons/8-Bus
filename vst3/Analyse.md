# 8-Bus – Schaltungsanalyse und Annahmen

Grundlage: Schaltplan der Kanalplatine eines analogen 8-Bus-Pults der 90er (Vorverstärker, EQ-Modul, Fader-Stufe).
Zum Abgleich dienen die technischen Daten aus der Bedienungsanleitung des Pults („Handbuch“ in den Tabellen).

## Signalweg eines Kanals (so im Plugin)

| Stufe | Schaltung | Clip-Punkt |
|---|---|---|
| Line-Eingang | 7K32 längs / 3K92 quer je Ader → −9,1 dB | – |
| Vorverstärker | Differenzpaar Q101–Q104, Gain-Widerstand R113 22 Ω + TRIM R166 5K (+ ~3 Ω Transistoren), C107 470 µF in Reihe; Strom durch Rg begrenzt auf den Ruhestrom je Hälfte (2K49 von +16 V ≈ 6,2 mA); U1A (2 × 3K92) | Differenzpaar (Strom), U1A, ±16 V |
| Lo Cut | Sallen-Key 3. Ordnung, C101–C103 0,22 µF, R101 5K1, R102 1K3, R103 62K, Puffer U104-22/23 | Puffer |
| Lo Mid | U2B invertierend 10K/10K, Plus-Eingang = Wien-Bandpass vom Schleifer R112 (10KB zwischen Eingang und U2B-Ausgang): R111A/B 500K + 8K2, C110 10 nF, C111 3,9 nF | U2B |
| Hi Mid | U3A invertierend 10K/10K, Plus-Eingang = Bandpass eines State-Variable-Filters (U4B, U3B, U4A; R164A/B 50K + 1K3, C118/C119 6,8 nF), angesteuert vom Schleifer R113 (50KB zwischen U2B- und U3A-Ausgang) über C139 18 nF + R16 11K; Bandbreite R115 50K (Schleifer an Masse) | U3A |
| Hi/Lo | Baxandall um U1B (HI: C105/C106 6,8 nF, R106 50K, R7 3K9; LOW: R105/R4 5K6, R107 50K, C107/C108 33 nF, R2 10K; R3 1M), danach Inverter U2A | U1B, U2A |
| Fader | 10K-Fader, danach U102B mit 1 + 1K1/510 = +10 dB | U102B, ±18 V |
| OL-LED | drei Spitzenwert-Abgriffe (1N4148 + 200K) auf C113 0,22 µF ∥ 100K, Komparator bei 2,78 V | Anzeige |

Koppelkondensatoren (47 µF) zwischen den Stufen sind als Hochpässe unter 1 Hz enthalten. Am Ausgang des Plugins
sitzt zusätzlich die Kopplung des Summen-/Wandlereingangs (Hochpass 2. Ordnung, 5 Hz).

## Übersteuern, Tail und „Ladung" (Version 0.1.1)

* **Tail:** Mit voll aufgedrehtem Trim (+41 dB) wird der natürliche Ausklang der Kick so weit angehoben, dass er
  lange an den Schienen clippt – die Kick hält 550 ms statt 100 ms, mit LO +15 und Fader +10 rund 900 ms
  (`doc/tail_vergleich.png`, `doc/klang/`). Länger als der Ausklang im Eingangssignal kann der Tail nicht werden:
  Endet ein Sample hart, endet auch der Tail dort (am echten Pult genauso).
* **Fehler in 0.1.0:** C107 war ein idealer Hochpass. Er konnte bei Übersteuerung beliebig schnell mitladen und gab
  die Ladung nach der Kick als langen Unterschall-Nachlauf ab. Die LO-Anhebung (Baxandall: +20 dB bei
  Unterschall) und Fader +10 machten daraus bis zu 8 V am Ausgang: Meter an, nichts zu hören, nächste heiße Stufe
  knackt. Jetzt lädt C107 nur über den begrenzten Strom des Differenzpaars (≤ 13 V/s), die Ausgangskopplung nimmt
  den Unterschall weg. Ergebnis: Nach einem hart endenden Sample bleibt ein einzelner weicher Nachschwinger,
  der in ~0,5 s ausklingt (vorher: Hängen an der Schiene bzw. Rest bei −52 dB).
* **Schienen:** leicht unsymmetrisch (+16,1 / −15,9 V Kanal, +14,6 / −14,4 V Vorverstärker); der entstehende
  Gleichanteil wird von den Koppel-Cs wie im Pult verarbeitet.
* Mehrere Kanäle mit vollem Trim hintereinander (über 80 dB Verstärkung) neigen auch im Modell zum „Blocken“ –
  am echten Pult würde so eine Kette außerdem stark rauschen und brummen.

## Abgleich mit dem Handbuch

| | Schaltung (gerechnet) | Handbuch |
|---|---|---|
| Lo Mid Frequenz | 50 Hz – 3,1 kHz | 45 Hz – 3 kHz |
| Lo Mid Bandbreite | 2,09 Oktaven (halbe Anhebung) | „fest 2 Oktaven" |
| Lo Mid Hub | ±15,7 dB | ±15 dB |
| Hi Mid Frequenz | 456 Hz – 18 kHz | 500 Hz – 18 kHz |
| Hi Mid Bandbreite | 3 / 2 / 1/12 Oktave | 3 Oktaven – 1/12, Mitte 2 |
| Hi Mid Hub | ±15,1 dB | ±15 dB |
| Hi 12 kHz / Lo 80 Hz | +15,2/−15,5 dB, +14,8/−15,5 dB | ±15 dB |
| Lo Cut | Tschebyscheff, 0,5 dB Welligkeit, −3 dB bei 85 Hz | 75 Hz, 18 dB/Okt., Tschebyscheff |
| Fader | +10 dB über U | „fader up +10" |

## Annahmen (nicht eindeutig aus den Unterlagen)

* **Poti-Kurven:** Frequenz-Potis (500KC, 50KC) als Log-Kurve mit 15 % bei Mittelstellung, Boost/Cut-Potis linear.
  Bandbreite (50KG): Kurve so gewählt, dass der Knopf 3 → 2 (Mitte) → 1/12 Oktave logarithmisch durchläuft.
* **Hi-Mid-Boost-Richtung:** Der Bandpass des SVF ist invertiert – Anhebung liegt am Poti-Ende zum Eingang (geprüft).
* **R20:** 3K6 (Modul) statt 3K3 (Kanalplatine) – ohne Einfluss, er führt keinen Strom.
* **C139 18 nF** mit 11K: Bei breiter Bandbreite wandert die tiefste Hi-Mid-Anhebung auf ~840 Hz (bei schmaler bis ~530 Hz).
  Das ist so in der Schaltung, nicht im Plugin erfunden.
* **R3 1M** parallel zur Baxandall-Rückkopplung: in Mittelstellung −0,4 dB im Bass (Schaltung so gezeichnet).
* **Trim:** Verstärkung aus der Schaltung abgeleitet: Line −5 … +41 dB, 0 dB bei ~17 % Drehung. Der genaue Wert der
  Stromrückkopplung (2 × 3K92) ist aus dem Plan erschlossen – Abgleich mit einer echten Aufnahme steht aus.
* **Pegel:** 1,0 digital = 8,69 V Spitze (0 dBu = −18 dBFS). Op-Amp-Ausgänge sättigen weich ab 92 % der Schiene:
  +16,2 / −15,8 V (Kanal, ±18 V), +14,8 / −14,4 V (Vorverstärker, ±16 V). Eine 0-dBFS-Kick clippt also erst ab
  ca. +5,5 dBFS Pegel innerhalb des Pults – Trim, EQ-Anhebung und Fader +10 dB bringen sie dorthin.
* **Sättigung:** je Op-Amp-Ausgang statisch (Filter dazwischen exakt). Innere Zustände bei Übersteuerung
  (Rückkopplung bricht weg), Slew-Rate und Übernahmeverzerrung sind nicht modelliert.
* **OL-LED-Abgriffe:** Vorverstärker-Ausgang, EQ-Ausgang, Fader-Ausgang (laut Bericht „drei wichtige Punkte").
* **Fader-Skala:** +10, +5, U, 5, 10, 20, 30, 40, 50, 60, ∞ (übliche Pult-Skala), stückweise linear in dB.

## Prüfung

* `tools/circuit_ref.py` (reines Python, MNA mit idealen Op-Amps) erzeugt `test/ref_curves.txt`.
* `r8_curves`: C++-Netzlisten = Python-Referenz (0,00000 dB), digitale Filter = analoge Kurve (0,0000).
* `r8_enginetest`: Pegel neutral −0,13 dB, Latenz 36 Samples, Clipping an der Schiene, OL-LED, Ketten,
  Tail (Trim voll ≥ 3× länger), kein Gleichanteil am Ausgang, Ausklingen nach hartem Sample-Ende,
  wilde Automation ohne NaN, Rechenzeit.
* `r8_hosttest`: Plugin wie in einer DAW laden, Klang, Bypass, Zustand.
* Steinberg-Validator (Linux und Windows/Wine): 537 Tests bestanden, 0 fehlgeschlagen.

## Flattern über Netzteil und Masse (Version 0.2.0)

Grundlage: eine Aufnahme eines echten Pults (nicht im Repository): 5 Kanäle in Kette, kurze 909-Kick alle 400 ms
(die Kick selbst ist nach ~180 ms vorbei). Gemessen:

| | entspannt | erster Trim hoch | zweiter Trim dazu |
|---|---|---|---|
| Nach der Kick | klingt in ~180 ms aus | Ton hält ~250 ms voll, dann −13 dB/50 ms | Ton hält bis zum nächsten Schlag |
| Tonhöhe | Kick 250 → 54 Hz | 54,3 Hz (18,4 ms) | 54,3 Hz, auch bei jedem weiteren Schlag |

Das Pult schwingt selbst: eine Schleife, deren Verstärkung mit dem Trim steigt (unter 1: langes Nachschwingen,
über 1: Durchschwingen, begrenzt durch das Clipping). Der Weg führt über die gemeinsame Versorgung:

* Netzteil (Board 15): ±18 V diskret geregelt (MJE3055/2955, je 2 × 0,51 Ω, Strombegrenzung ~2,5 A,
  22.000 µF Siebung) – Strom ist genug da, die Spannung am Netzteil ist steif.
* Geregelt wird aber im Netzteil, nicht im Pult; dazwischen liegen Kabel, Stecker, Stromverteilung und Masseschiene.
  Die Platinen haben nur Puffer-Cs (Vorverstärker-Board 2 × 470 µF an J12, keine eigene Regelung).
* Der langsame Regler (C33 47 µF, C9 470 pF im Regelkreis) wirkt oberhalb seiner Regelbandbreite wie eine
  Induktivität; mit ~10 mF Puffer-Cs aller Platinen gibt das eine Resonanz der Versorgungsimpedanz im Bass
  (~0,8 mH / 10 mF ≈ 56 Hz).
* Jede Op-Amp-Stufe treibt ihren Laststrom durch diese Impedanz (Lasten aus dem Plan, die Fader-Stufe U102B mit
  ~1,4 kΩ am stärksten; pro heißem Kanal ~±40 mA). Die Schienen sacken ab (Clip-Grenzen, Ruhestrom des
  Eingangs-Differenzpaars über 2K49), der Masse-Rückstrom hebt die Pultmasse gegen die Außenwelt an – der erste
  Line-Eingang verstärkt das mit seinem Trim.

Umsetzung (`src/core/supply.h`): gemeinsamer Versorgungsknoten je Audiokanal, alle Kanalzüge werden dazu Sample
für Sample gemeinsam gerechnet. Kalibriert an der Aufnahme (die Stärke der Kopplung steht in keinem Plan):
Resonanz 52 Hz / Q 2, Kopplung 8 Ω (wirksam am ersten Eingang), Schienen 0,15 Ω + 1 A Grundlast.
Regler **Netzteil/Masse**: aus (ideal) … ×1 (wie die Aufnahme, Grundstellung) … ×4.

Ergebnis im Modell (gleiche Kette, kurze Kick): entspannt unverändert; ein heißer Trim −10 dB statt −25 dB
nach 160 ms; zwei heiße Trims halten bis zum nächsten Schlag; Ton 54,1 Hz; Obertöne H3/H5/H7
−10 / −15 / −20 dB (Aufnahme −9 / −15 / −17 dB). Gerade Obertöne und Spitzigkeit der Wellenform sind im Modell
geringer – sie hängen am unbekannten EQ der hinteren Kanäle.

Wie am echten Pult: Ist die Schleife über 1, schwingt es auch ohne Eingang weiter (Motorboating), bis Trim
oder Regler zurückgenommen werden.

### Änderung in 0.3.0: kein Selbstschwingen

0.2.0 konnte bei viel Gesamtverstärkung (z. B. 8 heiße Kanäle) dauerhaft selbst schwingen – das Original tut das nicht.
Jetzt:
* Antrieb: Laststrom und vor allem der Spitzenstrom von Op-Amps in der Sättigung (Gegenkopplung offen, Treiber und
  Endstufe ziehen bis ~±20 mA). Im linearen Betrieb ist er klein.
* Die Masse-Spannung am Eingang ist an das ankommende Signal gebunden: höchstens 30 % seines Pegels, die Hüllkurve
  fällt mit 0,2 s und ist unter 1 mV ganz aus. Das Flattern kann also nie aus der Stille anschwingen und klingt
  ohne Eingang in ~1 s aus („Ausflattern“), egal wie hoch die Verstärkung ist.
* Kalibrierung bleibt (5 Kanäle): ein heißer Trim −10 dB statt −25 dB nach 160 ms, zwei heiße Trims −3 dB nach
  360 ms (hält bis zum nächsten Schlag), Ton 53,5 Hz.
* Grenze: Bei extremer Kettenverstärkung (8 Kanäle mit Trim ab ~+15 dB, also über 120 dB) verstärkt die Kette
  die Erholung ihrer eigenen Koppelkondensatoren so stark, dass es nach einer Kick noch ein bis zwei Sekunden
  „blockt“ – auch mit Regler auf „aus“. Das ist die Kette selbst, nicht das Netzteil-Modell.
