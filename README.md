<p align="center"><img src="resources/logo_full.png" alt="Artilody" height="90"></p>

# Artilody Tracker

En pattern-baseret music tracker skrevet i C++ med [JUCE](https://juce.com).
Til forskel fra de fleste trackere har Artilody Tracker en rigtig mixer: når du opretter et track,
oprettes der automatisk en channel strip **lige under tracket**, som styrer det.



## Funktioner

**Tracker**
- Patterns med op til 512 linjer, flere note-kolonner pr. track (akkorder), kolonnerne Note / Ins / Vol / FX
- Song sequence (order list), pattern-liste og pattern-egenskaber (navn, længde, farve, noter)
- Mini-vinduer til alle felter i edit mode: klaver til toner, instrumentliste, volumen-slider og effektvælger, MIDI-keyboard input (også akkorder), edit step, oktav, follow playback, loop pattern
- Copy / cut / paste, transponering, interpolering af volume, indsæt/slet linjer, undo/redo
- Effektkommandoer: `Txx` tempo, `Lxx` lines per beat, `Dxx` delay, `Cxx` cut, `Rxy` retrigger,
  `Pxx` pitch bend, `Mxx` modulation, `Bxx` hop i sekvensen

**Mixer**
- Master stereo channel strip
- En channel strip pr. track, placeret lige under tracket
- Hele mixeren kan kollapses (`Ctrl+M` eller *Hide Mixer*) for bedre overblik over trackene

**Channel strip**
1. **Instrument** – indbyggede genrer *Pop, Rock, Electro og Jazz* med forskellige SoundFont-instrumenter
   (trommesæt, bas, klaver, guitar, synth, blæsere …), alle presets i SoundFonten, egen `.sf2`,
   eller **VST3, AU (macOS) og CLAP** instrument plug-ins
2. **Plug-in effekter** – vilkårligt antal VST3 / AU / CLAP effekter; flyt op/ned, bypass, fjern,
   og kollaps listen så den fylder mindre
3. **Indbyggede effekter** – EQ (low cut + 3 bånd), kompressor og reverb; tænd/sluk og indstil
4. **Panorering**
5. **Lydstyrke** (fader med meter), mute og solo

Appen har en almindelig menulinje (File, Edit, Song, View, Plug-ins, Options, Help).
Projekter gemmes som `.artixml` – almindelig XML, inkl. plug-in-tilstand, sange kan eksporteres som WAV, AIFF eller FLAC,
og en lydfil (WAV/AIFF/FLAC/OGG/MP3) kan åbnes som sampler-instrument på det valgte track.




## Brugermanual

- Dansk brugermanual: [online](docs/Manual-da.md) · [PDF](docs/Manual-da.pdf)
- English user manual: [online](docs/Manual-en.md) · [PDF](docs/Manual-en.pdf)

PDF-udgaverne genereres fra Markdown-filerne med `docs/tools/build-pdf.sh` (macOS).

Begge manualer har et appendiks, der beskriver opbygningen af `.artixml`-filformatet.

## Installation

Hent den nyeste version under [Releases](https://github.com/shenphenchoedron-hue/Artilody-Tracker/releases):
Windows installer, macOS DMG samt `.deb`-pakker til Debian 12, Ubuntu 22.04 og Ubuntu 24.04.

## Tastatur

| Tast | Funktion |
|------|----------|
| `Esc` | Edit mode til/fra |
| `Space` / `Shift+Space` | Afspil (fortsæt) sang / loop pattern eller loop-rækker (stop hvis den spiller) |
| Træk over rækkenumrene | Markér rækker, der skal loopes (dobbeltklik fjerner) |
| `Enter` | Afspil fra markørens linje |
| Klik på et felt (edit mode) | Mini-vindue til tone, instrument, volume eller effekt |
| MIDI-keyboard (edit mode) | Skriv toner (flere tangenter = akkord) |
| `0-9`, `A-F` | Hex-værdier i Ins / Vol / FX |
| Piletaster, `Tab`, `PgUp/PgDn`, `Home/End` | Navigation |
| `Shift` + piletaster / mus-træk | Markering |
| `Del`, `Ins`, `Backspace` | Slet, indsæt linje, slet linje |
| `Ctrl+C/X/V`, `Ctrl+Z/Y` | Udklipsholder, fortryd |
| `Ctrl+↑/↓` (`+Shift` = oktav) | Transponér |
| `Num *` / `F2` | Standardoktav op |
| `Num /` / `F1` | Standardoktav ned |
| `Ctrl+N/O/S`, `Ctrl+Shift+S` | Nyt projekt, åbn, gem, gem som |
| `Ctrl+I` / `Ctrl+E` | Åbn lydfil som instrument / eksporter lyd |
| `Ctrl+T/M/P/Q` | Nyt track, mixer, plug-in manager, afslut |

## Byg selv

Kræver CMake ≥ 3.22 og en C++17-compiler. JUCE hentes automatisk.

```bash
# Debian/Ubuntu afhængigheder
sudo apt install build-essential cmake libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev

# valgfrit: den medfølgende General MIDI SoundFont
mkdir -p soundfonts
curl -L -o soundfonts/GeneralUser-GS.sf2 \
  https://raw.githubusercontent.com/mrbumpy409/GeneralUser-GS/main/GeneralUser-GS.sf2

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && cpack -G DEB   # Linux-pakke
```

Uden den medfølgende SoundFont bruger appen en system-SoundFont (fx `/usr/share/sounds/sf2/FluidR3_GM.sf2`),
eller du kan vælge en under *Options → Default SoundFont*.

## Licens

Kildekoden er under GPLv3 (se `LICENSE`). Binærfilerne linker til open source-udgaven af JUCE og er derfor
også underlagt AGPLv3. Se [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for tredjepartskomponenter.
