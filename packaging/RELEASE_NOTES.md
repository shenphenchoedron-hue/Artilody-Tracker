## Artilody Tracker

Pattern-baseret music tracker med fuld mixer – hvert track har sin egen channel strip lige under sig.

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
