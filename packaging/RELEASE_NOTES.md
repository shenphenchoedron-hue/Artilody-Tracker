## Artilody Tracker

Pattern-baseret music tracker med fuld mixer – hvert track har sin egen channel strip lige under sig.

### Nyt i 1.0.7
- **Eksport til MIDI** (*File → Export Song as MIDI File…*): standard MIDI-fil med ét spor pr. instrument,
  General MIDI-programmer, trommer på kanal 10, tempo, pitch bend og modulation.
- **Eksport til MusicXML** (*File → Export Song as MusicXML…*): noder til MuseScore, Dorico, Finale, Sibelius
  m.fl. med én stemme pr. instrument, akkorder, bindebuer over taktstreger og tempoangivelser.
- Manualerne er opdateret.

### Nyt i 1.0.6
- **Parametrisk EQ:** op til 8 frit placerede bånd (IIR-biquad: Bell, Low/High Shelf, Low/High Cut, Notch).
  Klik på linjen for at sætte et punkt, træk det op/ned og til siden, Shift = fin justering, musehjul = Q.
  Ændringer under afspilning glider blødt uden klik. Gamle projekter omdannes automatisk.
- **Mini-vinduer til alle felter** i edit mode: klaver med oktavvalg til toner (inkl. Note off),
  instrumentliste, volumen-slider med dynamik-knapper og en effektvælger, der forklarer værdien i klartekst.
- Computertastaturet spiller ikke længere toner (tasterne svarede ikke til tonernes navne).
- **MIDI-keyboard:** keyboards tilsluttet under kørslen findes automatisk, og akkorder skrives i flere
  note-kolonner.
- **Edit: On/Off** og **Loop: On/Off** knapper i værktøjslinjen.
- **Loop af rækker:** træk over rækkenumrene (eller klik og Shift+klik) for at markere rækker, der skal loopes.
- **Play fortsætter** fra der, hvor man trykkede Stop.
- Rød stop-knap, større rund optageknap der lyser og pulserer i edit mode, tydeligt loop-ikon.
- Manualerne er opdateret.

### Nyt i 1.0.5
- **Scan for new plug-ins**: én knap i Plug-in Manager (og menupunktet *Plug-ins → Scan for New Plug-ins*)
  scanner VST3, AU og CLAP i ét hug. Kun nye eller opdaterede plug-ins indlæses.
- Nyt design af Plug-in Manager: luft i siderne, oversigt over antal plug-ins, statuslinje, farvede
  format-mærker (VST3 / AU / CLAP), tydelig markering af plug-ins der fejlede under scanning.
- Manualerne er opdateret.

### Nyt i 1.0.4
- **macOS:** Rettet nedbrud under plug-in-scanning med plug-ins, der skal indlæses på hovedtråden (fx Native
  Instruments Komplete Kontrol). Plug-in Manager scanner nu på hovedtråden.
- Brugermanualerne som PDF følger nu med i alle pakker.

### Nyt i 1.0.3
- Projekter gemmes nu som **`.artixml`**, så det er tydeligt, at filen er almindelig XML. Ældre `.artilody`-filer
  kan stadig åbnes og gemmes som `.artixml`.
- `.artixml` er tilknyttet programmet på Windows og macOS (dobbeltklik åbner projektet).
- Ny brugermanual på dansk og engelsk (`Manual-da.pdf` / `Manual-en.pdf`, også som Markdown) med et appendiks,
  der beskriver `.artixml`-formatet. Manualerne følger med i alle pakker.
- Rettet hjælpeteksten for oktavskift (F1 = ned, F2 = op).

### Nyt i 1.0.2
- **macOS:** Rettet skrattende/hakkende lyd under afspilning. Pattern-griddet holdt mixerens lås, mens det blev
  tegnet, så lydtråden måtte vente på tegningen ved hver ny linje. Griddet, undo og gem holder ikke længere låsen,
  og lydtråden allokerer ikke længere hukommelse ved forsinkede noder.

### Download

| Platform | Fil |
|----------|-----|
| Windows 10/11 (64-bit) | `ArtilodyTracker-*-Windows-x64-Setup.exe` (installer) eller `*-portable.zip` |
| macOS 11+ (Apple Silicon + Intel) | `ArtilodyTracker-*-macOS-universal.dmg` |
| Debian 12 | `artilody-tracker_*_debian12_amd64.deb` |
| Ubuntu 22.04 | `artilody-tracker_*_ubuntu22.04_amd64.deb` |
| Ubuntu 24.04 | `artilody-tracker_*_ubuntu24.04_amd64.deb` |

**Debian / Ubuntu:** `sudo apt install ./artilody-tracker_*_amd64.deb`

**macOS:** Appen er ikke notariseret. Træk den til Programmer, højreklik på den og vælg *Åbn* første gang
(eller kør `xattr -dr com.apple.quarantine "/Applications/Artilody Tracker.app"`).

**Windows:** SmartScreen kan advare om en ukendt udgiver – vælg *Flere oplysninger → Kør alligevel*.

### Funktioner
- Tracker-editor med flere note-kolonner pr. track, hex-værdier, effektkommandoer (T, L, D, C, R, P, M, B), copy/paste, transponering og undo
- Song sequence + pattern-liste med navne, farver, længder og noter
- Mixer med master channel strip og en channel strip pr. track (kan kollapses)
- Instrumentvalg: indbyggede Pop-, Rock-, Electro- og Jazz-instrumenter (SoundFont, GeneralUser GS medfølger) eller VST3 / AU / CLAP plug-ins
- Ubegrænset antal VST3 / AU / CLAP effekter pr. strip – kan flyttes op/ned, bypasses og listen kan kollapses
- Indbygget EQ, kompressor og reverb pr. strip (tænd/sluk + indstillinger)
- Panorering, lydstyrke, mute og solo
- Menulinje med File / Edit / Song / View / Plug-ins / Options / Help
- Åbn lydfiler (WAV/AIFF/FLAC/OGG/MP3) som sampler-instrument, eksport til WAV/AIFF/FLAC
- MIDI-keyboard input, projekter (.artixml – almindelig XML) og seneste projekter
- Demo-sange: *Sunrise Circuits* og jazznummeret *Blue Corner Swing*
