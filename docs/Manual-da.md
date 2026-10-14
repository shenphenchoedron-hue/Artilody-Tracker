# Artilody Tracker – Brugermanual

*Version 1.0.3 · [English version](Manual-en.md)*

Artilody Tracker er en pattern-baseret music tracker. Musikken skrives som noder i lodrette kolonner, der
afspilles linje for linje oppefra og ned. Til forskel fra de fleste trackere har hvert track sin egen
**channel strip** (mixerkanal) lige under sig med instrument, effekter, EQ, kompressor, reverb, panorering
og lydstyrke.

---

## Indhold

1. [Installation og første start](#1-installation-og-første-start)
2. [Grundbegreber](#2-grundbegreber)
3. [Vinduets opbygning](#3-vinduets-opbygning)
4. [Topbjælken og transport](#4-topbjælken-og-transport)
5. [Tracks](#5-tracks)
6. [Pattern-editoren](#6-pattern-editoren)
7. [Effektkommandoer](#7-effektkommandoer)
8. [Sekvens og patterns](#8-sekvens-og-patterns)
9. [Mixeren og channel strips](#9-mixeren-og-channel-strips)
10. [Instrumenter](#10-instrumenter)
11. [Plug-ins (VST3, AU, CLAP)](#11-plug-ins-vst3-au-clap)
12. [Indbyggede effekter](#12-indbyggede-effekter)
13. [Projekter, eksport og indstillinger](#13-projekter-eksport-og-indstillinger)
14. [Menuer](#14-menuer)
15. [Tastaturgenveje](#15-tastaturgenveje)
16. [Grænser og fejlfinding](#16-grænser-og-fejlfinding)
- [Appendiks A – Filformatet `.artixml`](#appendiks-a--filformatet-artixml)

---

## 1. Installation og første start

Hent den nyeste version under *Releases* på GitHub:

| Platform | Fil |
|---|---|
| Windows 10/11 (64-bit) | `ArtilodyTracker-*-Windows-x64-Setup.exe` eller `*-portable.zip` |
| macOS 11+ (Apple Silicon og Intel) | `ArtilodyTracker-*-macOS-universal.dmg` |
| Debian 12 / Ubuntu 22.04 / 24.04 | `artilody-tracker_*_<distro>_amd64.deb` |

- **macOS:** Træk appen til *Programmer*. Appen er ikke notariseret, så første gang skal du højreklikke og
  vælge *Åbn* (eller køre `xattr -dr com.apple.quarantine "/Applications/Artilody Tracker.app"`).
- **Windows:** SmartScreen kan advare om en ukendt udgiver – vælg *Flere oplysninger → Kør alligevel*.
- **Debian/Ubuntu:** `sudo apt install ./artilody-tracker_*_amd64.deb`

Ved første start indlæses demosangen *Sunrise Circuits*. Tryk på **Space** for at høre den.

Den medfølgende SoundFont *GeneralUser GS* leverer de indbyggede instrumenter. Mangler den, kan du vælge
en anden `.sf2`-fil under *Options → Default SoundFont…*.

Lydkortet vælges under *Options → Audio & MIDI Settings…* (eller ved at klikke på *Main Out* i master-stripen).
Hvis lyden hakker, så hæv **buffer size** (fx 512 eller 1024 samples).

---

## 2. Grundbegreber

| Begreb | Forklaring |
|---|---|
| **Track** | En lodret stribe i editoren. Hvert track har sin egen channel strip og sit eget instrument. |
| **Note-kolonne** | Et track kan have 1–8 note-kolonner, så det kan spille akkorder. Hver kolonne spiller én tone ad gangen. |
| **Linje (row)** | En vandret række. Linjerne afspilles en ad gangen oppefra og ned. |
| **Pattern** | Et stykke musik på 1–512 linjer, der dækker alle tracks. |
| **Sekvens** | Rækkefølgen af patterns i sangen (order list). Det samme pattern kan bruges flere gange. |
| **BPM** | Tempo i slag pr. minut. |
| **LPB** | *Lines per beat* – hvor mange linjer der går på ét slag. Ved LPB 4 er hver linje en 1/16-node. |
| **Edit mode** | Når edit mode er slået til, skrives tasterne ind i patternet. Ellers spiller de kun tonen. |

Alle tal i patterns (instrument, volume og effektværdier) vises **hexadecimalt** (00–FF), som det er
skik i trackere.

---

## 3. Vinduets opbygning

```
┌──────────────────────────── Topbjælke (transport, tempo, meter, projekt) ───────────────────────────┐
├─ + Add Track │ Columns − + │ Pattern-info ───────────────────────── Collapse FX lists │ Hide Mixer ─┤
├──────┬─────┬──────────────────────────────────────────────────────────────┬──────────────────────────┤
│ Seq  │ Row │  Track-faneblade   01 Drums │ 02 Bass │ 03 Chords │ +         │ Patterns                 │
│      │ nr. │  Note Ins Vol FX ...                                         │                          │
│ 00 ▶ │ 00  │  C-4  01  7F ---                                             │ Pattern Properties       │
│ 01   │ 01  │  ---  --  -- ---   (pattern-grid)                            │  Name, Length,           │
│ ...  │ ... │                                                              │  Colour, Notes           │
│      │     ├──────────────────────────────────────────────────────────────┤                          │
│Master│     │  Channel strip for hvert track (lige under tracket)          │ Keys (hjælp)             │
└──────┴─────┴──────────────────────────────────────────────────────────────┴──────────────────────────┘
```

- **Topbjælken** (øverst): transport, tempo, oktav, edit step, master-meter, CPU, undo/redo og projektnavn.
- **Værktøjslinjen**:
  - **+ Add Track**
  - **Columns − / +**: antal note-kolonner på det valgte track
  - en info-boks med patternets nummer og navn samt markørens track og linje
  - **Collapse FX lists** og **Hide Mixer / Show Mixer**
- **Venstre kolonne**: sekvenslisten (*Seq*), linjenumre (hex) og nederst **master-stripen**.
- **Midten**: track-faneblade, pattern-griddet og en channel strip under hvert track. Området kan rulles
  vandret, hvis der er mange tracks.
- **Højre panel**: pattern-listen, pattern-egenskaber og en kort tastaturhjælp.

Når **edit mode** er slået til, får griddet en rød ramme, og markøren bliver rød.

---

## 4. Topbjælken og transport

| Knap / felt | Funktion |
|---|---|
| **Rewind** | Går til starten af sangen (sekvens-plads 0, linje 0) uden at starte afspilning. |
| **Play** | Afspiller sangen fra den valgte sekvens-plads (**Space**). |
| **Loop** | Afspiller det aktuelle pattern i loop (**Shift+Space**). |
| **Stop** | Stopper og slukker alle toner. |
| **Record** | Slår edit mode til/fra (**Esc**). |
| **BPM** | Tempo, 20–999. Viser det aktuelle tempo under afspilning, også når `Txx`-effekter ændrer det. |
| **LPB** | Linjer pr. slag, 1–32. |
| **Oct** | Grundoktav for tastaturet, 0–9 (standard 4). |
| **Step** | Edit step, 0–16: hvor mange linjer markøren flytter sig, efter du har skrevet en værdi. 0 = bliver stående. |
| **Follow** | Når den er tændt, følger griddet afspilningen. |
| **MASTER / CPU** | Master-udgangens niveaumeter og lydmotorens CPU-forbrug. |
| **Undo / Redo** | Fortryd / gentag (**Ctrl+Z / Ctrl+Y**). |
| **Project** | Sangens titel. Der står *(modified)*, når der er ændringer, som ikke er gemt. |

**Talfelterne** (BPM, LPB, Oct, Step) kan ændres på fire måder:

- **Træk** op eller ned med musen.
- **Rul** med musehjulet (±1).
- **Klik** én gang for at vælge en forudindstillet værdi i en menu.
- **Dobbeltklik** for at skrive en præcis værdi.

**Afspilning fra tastaturet:**

- **Space**: afspil / stop.
- **Shift+Space**: loop pattern.
- **Enter**: afspil fra den linje, markøren står på.

---

## 5. Tracks

**Opret et track** med en af disse:

- **+ Add Track**
- **+** til højre for det sidste faneblad
- **Ctrl+T**
- *Song → Add Track*

Det nye track får instrumentet *Pop / Grand Piano*. Der kan højst være 64 tracks.

**Faneblad:**

- **Klik** vælger tracket.
- **Dobbeltklik** omdøber det (højst 24 tegn).
- **Højreklik** eller **…**-knappen åbner track-menuen.

**Track-menuen:**

| Punkt | Funktion |
|---|---|
| Rename… | Omdøb tracket. |
| Colour | Vælg en af 10 farver. |
| Note columns | 1–8 note-kolonner (også med **Columns − / +** i værktøjslinjen). |
| Choose instrument… | Åbner instrumentmenuen (se afsnit 10). |
| Add plug-in effect… | Tilføj en plug-in-effekt. |
| Move left / Move right | Flyt tracket. Ins-numre i alle patterns rettes automatisk. |
| Insert track after | Indsæt et tomt track (uden instrument) efter dette. |
| Delete track | Slet tracket og dets channel strip (kræver bekræftelse). |

> Flytning, indsætning og sletning af tracks rydder fortryd-historikken.

---

## 6. Pattern-editoren

### Kolonnerne

Hver note-kolonne har fire felter:

```
C-4 01 7F D80
│   │  │  └─ FX:   effektkommando (bogstav) + værdi (hex)
│   │  └──── Vol:  lydstyrke/anslag 00–7F
│   └─────── Ins:  instrument (track-nummer, hex)
└─────────── Note: tone, OFF (note off) eller --- (tom)
```

- **Note** vises som navn og oktav, fx `C-4`, `F#5`. Tone 60 (MIDI's midter-C) hedder `C-5`.
- **Ins** angiver hvilket tracks instrument, tonen skal spilles på: `01` = track 1, `0A` = track 10.
  - Er feltet tomt (eller `00`/ugyldigt), spiller tonen på trackets eget instrument.
  - Lyden går gennem det valgte instruments channel strip.
  - Når du skriver en tone i et tomt Ins-felt, udfyldes det automatisk med trackets eget nummer.
- **Vol** er anslagsstyrken. `7F` er fuld styrke; tomt felt svarer til ca. `66` (80 %).
- **FX** er en effektkommando (se afsnit 7).

Linjer, der starter et slag (hver LPB-linje), har en lysere baggrund og fed skrift i linjenummeret.

### Skriv noder

1. Tryk **Esc** for at slå edit mode til (rød ramme).
2. Spil på computertastaturet, der er indrettet som et klaver:

```
Øvre række (oktav +1):   2 3   5 6 7   9 0
                        Q W E R T Y U I O P
Nedre række (grundoktav): S D   G H J   L
                        Z X C V B N M , .
```

- **Z** = C i grundoktaven (*Oct*), **S** = C#, **X** = D … **M** = H (B).
- **Q** = C en oktav højere.
- **1** eller **A** skriver **OFF**: den foregående tone i kolonnen slippes.
- Med standardoktav 4 er **Z** = `C-4` og **Q** = `C-5`.
- Skift oktav med **F1/F2** eller **Num / / Num \***.

Noder høres altid, også uden edit mode, men de skrives kun ind i patternet, når edit mode er tændt. Når
en tone er skrevet, flytter markøren sig *Step* linjer ned.

**MIDI-keyboard:** Alle MIDI-indgange er aktive. Toner spilles på markørens track, og i edit mode skrives de
ind med anslaget som Vol-værdi.

### Hex-værdier

Placér markøren i Ins-, Vol- eller FX-værdi-feltet, og skriv **0–9** og **A–F**:

- Hvert felt har to cifre (høj og lav). Markøren hopper ned efter hvert ciffer.
- I FX-kommandofeltet skrives et bogstav, fx **T** eller **D**.
- **Del** rydder feltet: i notefeltet hele cellen, ellers kun det felt, markøren står i.

### Navigation og markering

| Tast | Funktion |
|---|---|
| ↑ / ↓ | En linje op/ned (går rundt fra bund til top) |
| PgUp / PgDn | 4 slag (16 linjer ved LPB 4) |
| Home / End | Første / sidste linje |
| ← / → | Forrige / næste felt, videre til næste kolonne og track |
| Tab / Shift+Tab | Næste / forrige track |
| Ctrl+← / Ctrl+→ | Forrige / næste track |
| Shift + piletaster | Markér. Shift+←/→ markerer en hel note-kolonne ad gangen |
| Musehjul | Ruller 2 linjer. Shift+hjul ruller vandret mellem tracks |
| Klik / Shift+klik / træk | Placér markøren / udvid markering / markér et område |

### Redigering

| Tast | Funktion |
|---|---|
| Ctrl+C / X / V | Kopiér / klip / indsæt (markering eller enkelt celle). Indsæt starter ved markøren. |
| Ctrl+A | Markér hele tracket |
| Ctrl+↑ / ↓ | Transponér ±1 halvtone |
| Ctrl+Shift+↑ / ↓ | Transponér ±1 oktav |
| Del | Ryd markering eller felt |
| Ins | Indsæt tom linje i den aktuelle note-kolonne (edit mode) |
| Backspace | Slet linje i den aktuelle note-kolonne og træk resten op (edit mode) |
| Ctrl+Z / Ctrl+Y | Fortryd / gentag |

Højreklik i griddet giver de samme kommandoer. Desuden kan du vælge **Interpolate volume** (lineær
overgang i Vol mellem første og sidste linje i markeringen) og se en oversigt over **Effect commands**.

---

## 7. Effektkommandoer

Effekter skrives i FX-kolonnen som ét bogstav og en hex-værdi `xx`.

| Kommando | Funktion | Eksempel |
|---|---|---|
| `Txx` | Sæt tempo til `xx` BPM (hex, 20–FF = 32–255 BPM) | `T78` = 120 BPM |
| `Lxx` | Sæt linjer pr. slag (01–20 hex = 1–32) | `L08` |
| `Dxx` | Forsink tonen med `xx`/256 af en linje | `D80` = en halv linje (swing) |
| `Cxx` | Skær tonen af efter `xx`/256 af en linje | `C40` = staccato |
| `Rxy` | Gentag tonen hver `y` tick (12 ticks pr. linje). `x` = hvor meget lydstyrken falder pr. gentagelse (x/16) | `R03` = 4 slag pr. linje |
| `Pxx` | Pitch bend: 00 = ned, 80 = midt, FF = op | `P80` |
| `Mxx` | Modulationshjul (MIDI CC1), 00–7F | `M40` |
| `Bxx` | Hop til sekvens-plads `xx` efter denne linje (ikke i loop pattern) | `B00` |

Tempo og LPB nulstilles til sangens værdier, når afspilningen stoppes og startes igen.

---

## 8. Sekvens og patterns

### Sekvenslisten (*Seq*, til venstre)

Hver plads viser sit nummer og nummeret på det pattern, den spiller, i patternets farve.

- **Klik** på en plads viser dens pattern.
- **+** indsætter et nyt pattern efter den valgte plads.

**Højreklik-menuen:**

| Punkt | Funktion |
|---|---|
| Insert new pattern after | Nyt tomt pattern efter pladsen |
| Repeat this pattern after | Samme pattern én gang til |
| Duplicate pattern into new slot | Kopi af patternet i en ny plads |
| Use pattern | Vælg hvilket pattern pladsen spiller |
| Move up / Move down | Flyt pladsen |
| Remove slot | Fjern pladsen |
| Play from here | Afspil sangen fra denne plads |

### Pattern-listen (højre panel)

- **Klik** vælger et pattern.
- **Dobbeltklik** sætter det ind i den valgte sekvens-plads.
- **+** opretter et nyt pattern uden at sætte det i sekvensen.

**…-menuen:**

- *Duplicate pattern*
- *Clear pattern*
- *Use in selected sequence slot*
- *Append to end of sequence*
- *Delete pattern*: fjerner også alle sekvens-pladser, der bruger patternet.

### Pattern Properties

| Felt | Funktion |
|---|---|
| **Name** | Patternets navn. |
| **Length** | 8–512 linjer. Når et pattern gøres kortere, slettes linjerne efter den nye slutning, og fortryd-historikken ryddes. |
| **Colour** | Farve i sekvens- og pattern-listen. |
| **Notes** | Fri tekst, fx akkorder eller idéer. |

---

## 9. Mixeren og channel strips

Under hvert track sidder en **channel strip**. Lyden løber gennem stripen i denne rækkefølge:

```
Instrument → plug-in-effekter (i rækkefølge) → EQ → Kompressor → Reverb → Pan / Volume → Master
```

Stripen indeholder oppefra:

1. **Hoved** med track-nummer, navn og **…**-menu (samme menu som track-fanebladet).
2. **Instrumentknap**, der viser instrumentets navn og type.
   - **Klik** åbner instrumentmenuen.
   - **Dobbeltklik** eller **Alt+klik** åbner plug-in-instrumentets editor.
3. **PLUG-INS**: listen af plug-in-effekter med **+** (tilføj) og en pil (kollaps listen).
   - **Klik** på en effekt åbner dens editor.
   - Tænd/sluk-ikonet bypasser effekten.
   - ▲ ▼ ✕ flytter op, flytter ned og fjerner (vises, når musen er over rækken). Det samme kan gøres fra
     højreklik-menuen.
   - Under listen ligger de indbyggede effekter **EQ**, **Comp** og **Reverb** (se afsnit 12).
4. **Fader** (−60 til +6 dB) med stereo-meter. Dobbeltklik nulstiller til 0 dB.
5. **Pan-knap** (L–C–R). Dobbeltklik centrerer.
6. **M** (mute) og **S** (solo). Er et track soloet, høres kun de soloede tracks.

### Master-stripen

Master-stripen nederst til venstre modtager summen af alle tracks:

- Den har plug-in-effekter, EQ, kompressor, reverb, volume, pan og mute, men ingen solo.
- Instrumentknappen viser **Main Out** og lydkortet. Klik på den åbner lydindstillingerne.
- Menuen har punkterne *Reset master strip* og *Audio settings…*.

### Skjul mixeren

**Hide Mixer**, **Ctrl+M** eller pilen over master-stripen gør alle strips små. Tilbage er kun navn, M/S,
en vandret fader og et meter, så der er mere plads til patterns.

**Collapse FX lists** kollapser eller udfolder plug-in-listerne i alle strips på én gang.

---

## 10. Instrumenter

Klik på instrumentknappen i en channel strip for at åbne instrumentmenuen.

- **Pop / Rock / Electro / Jazz**: kuraterede instrumenter fra den indbyggede SoundFont, fx trommesæt
  (Pop, Room, Rock, Electronic, 808, Jazz, Brush), klaver, Rhodes, bas, guitarer, synth, pads, blæsere og
  vibrafon.
- **All presets: …**: alle presets i standard-SoundFonten, sorteret efter bank. Trommesæt ligger i bank 128.
- **Load SoundFont file (.sf2)…**: brug din egen SoundFont. Den første preset i filen vælges.
- **Load audio file as sampler…**: brug en lydfil (WAV, AIFF, FLAC, OGG, MP3) som instrument.
  - Samplen spilles kromatisk med grundtone `C-5`, dvs. tone 60: tasten **Q** ved oktav 4.
  - Sampleren har 24 stemmer, og lydfiler på op til 120 sekunder indlæses.
  - Samme funktion: *File → Open Audio File as Instrument…* (**Ctrl+I**).
- **Instrument plug-ins**: VST3-, AU- (macOS) og CLAP-instrumenter, sorteret efter producent. Editoren
  åbnes automatisk.
- **Open instrument editor** / **Remove instrument**.

> Lydfiler og egne SoundFonts gemmes som **stier** i projektet, ikke som selve filen. Flytter du filerne,
> skal de vælges igen.

---

## 11. Plug-ins (VST3, AU, CLAP)

Åbn **Plug-in Manager** med **Ctrl+P** eller *Plug-ins → Plug-in Manager…*, og scan efter plug-ins:

- **Scan for new plug-ins** øverst i vinduet scanner VST3, AU og CLAP i ét hug. Det samme gør
  *Plug-ins → Scan for New Plug-ins (all formats)*.
  - Kun nye eller opdaterede plug-ins indlæses, så en ny scanning går hurtigt.
  - Mens der scannes, reagerer vinduet ikke på klik.
- Listen viser navn, format (VST3/AU/CLAP), type, producent og version. Plug-ins, der gik ned under
  scanning, er markeret med **FAILED** og springes over næste gang. Højreklik på et plug-in for at fjerne det
  eller vise dets mappe.
- Under **Options…** kan du scanne ét format ad gangen og ændre, hvilke mapper der scannes. Mapperne bruges
  også af *Scan for new plug-ins*.

- **VST3** på alle platforme, **Audio Unit** på macOS og **CLAP** på alle platforme.
- CLAP søges i standardmapperne og i mapper fra miljøvariablen `CLAP_PATH`:
  - macOS: `/Library/Audio/Plug-Ins/CLAP`, `~/Library/Audio/Plug-Ins/CLAP`
  - Windows: `%COMMONPROGRAMFILES%\CLAP`
  - Linux: `~/.clap`, `/usr/lib/clap`
- Får et plug-in programmet til at gå ned under scanning, springes det over ved næste scanning.

Plug-ins vælges derefter i instrumentmenuen eller med **+** i PLUG-INS-listen. Hvert plug-ins tilstand
gemmes i projektet. Plug-ins uden egen editor får en simpel parameter-editor.

---

## 12. Indbyggede effekter

Klik på **EQ**, **Comp** eller **Reverb** i en strip for at åbne effektens editor:

- Effekten tændes automatisk, når du drejer på en knap.
- Dobbeltklik på en knap nulstiller den.
- **Reset** sætter alle knapper til standard.

**EQ** (low cut + 3 bånd) har en frekvenskurve, der viser resultatet:

| Parameter | Område | Standard |
|---|---|---|
| Low Cut | 20–1000 Hz | 20 Hz (slukket) |
| Low Freq / Low Gain | 30–600 Hz / ±18 dB | 120 Hz / 0 dB |
| Mid Freq / Mid Gain / Mid Q | 200–8000 Hz / ±18 dB / 0,2–6 | 1 kHz / 0 dB / 0,9 |
| High Freq / High Gain | 2–16 kHz / ±18 dB | 8 kHz / 0 dB |

**Compressor** har et meter, der viser gain reduction:

| Parameter | Område | Standard |
|---|---|---|
| Threshold | −60–0 dB | −18 dB |
| Ratio | 1:1–20:1 | 4:1 |
| Attack | 0,1–200 ms | 10 ms |
| Release | 10–1500 ms | 150 ms |
| Knee | 0–24 dB | 6 dB |
| Makeup | 0–24 dB | 0 dB |

**Reverb:**

| Parameter | Område | Standard |
|---|---|---|
| Size | 0–1 | 0,55 |
| Damping | 0–1 | 0,45 |
| Width | 0–1 | 1,00 |
| Mix | 0–1 | 0,25 |

---

## 13. Projekter, eksport og indstillinger

- **Gem**: *File → Save Project* (**Ctrl+S**) / *Save Project As…* (**Ctrl+Shift+S**).
  - Projekter gemmes som **`.artixml`**, en almindelig XML-fil (se appendiks A).
  - Filen indeholder sangen, alle patterns, alle channel strips og plug-in-tilstande.
- **Åbn**: *File → Open Project…* (**Ctrl+O**) eller *Open Recent Project*. På macOS og Windows kan du
  også dobbeltklikke på en `.artixml`-fil.
- **Ældre projekter**: Filer fra version 1.0.0–1.0.2 hedder `.artilody` og kan stadig åbnes. Når du gemmer,
  skrives en ny `.artixml`-fil ved siden af. Den gamle fil røres ikke.
- **Eksport**: *File → Export Song as Audio…* (**Ctrl+E**) gemmer hele sangen som **WAV, AIFF eller
  FLAC**.
  - Lyden er 24-bit i lydkortets samplerate.
  - Sekvensen spilles én gang, plus 2,5 sekunders efterklang.
  - Eksporten kan annulleres undervejs.
- **Demosange**: *File → Demo Songs* indeholder *Sunrise Circuits* (pop/electro) og *Blue Corner Swing*
  (jazz).
- **Nyt projekt** (**Ctrl+N**) opretter fire tracks:
  - Drums (Pop Drum Kit)
  - Bass (Finger Bass)
  - Chords (Grand Piano med 3 kolonner)
  - Lead (Square Lead)

**Indstillinger** gemmes automatisk i filen `Artilody Tracker.settings`:

| System | Placering |
|---|---|
| macOS | `~/Library/Application Support/Artilody Tracker/` |
| Windows | `%APPDATA%\Artilody Tracker\` |
| Linux | `~/.config/artilody-tracker/` |

Filen indeholder lydkort, seneste projekter, seneste mappe, standard-SoundFont, mixer-tilstand og
listen over scannede plug-ins.

---

## 14. Menuer

| Menu | Indhold |
|---|---|
| **File** | New Project, Open Project…, Open Recent Project, Save Project, Save Project As…, Open Audio File as Instrument…, Export Song as Audio…, Demo Songs, Quit |
| **Edit** | Undo, Redo, Cut, Copy, Paste, Delete, Select Track Column, Transpose ±1 / ±12, Interpolate Volume, Edit Mode |
| **Song** | Play Song, Play From Start, Go to Start, Loop Pattern, Stop, Follow Playback, Add Track, Rename Song… |
| **View** | Show Mixer, Collapse / Expand All Plug-in Lists |
| **Plug-ins** | Plug-in Manager (scan VST3 / AU / CLAP)…, Scan for New Plug-ins (all formats) |
| **Options** | Audio & MIDI Settings…, Default SoundFont… |
| **Help** | Keyboard Shortcuts, About Artilody Tracker |

---

## 15. Tastaturgenveje

På macOS bruges **Cmd** i stedet for **Ctrl**.

| Tast | Funktion |
|---|---|
| Esc | Edit mode til/fra |
| Space / Shift+Space | Afspil sang / loop pattern (stop, hvis den spiller) |
| Enter | Afspil fra markørens linje |
| Z…M, Q…P | Noder (to oktaver) |
| A eller 1 | Note off |
| 0–9, A–F | Hex-værdier i Ins / Vol / FX |
| Piletaster, Tab, PgUp/PgDn, Home/End | Navigation |
| Shift + piletaster / træk med musen | Markering |
| Del / Ins / Backspace | Ryd, indsæt linje, slet linje |
| Ctrl+C / X / V | Kopiér, klip, indsæt |
| Ctrl+A | Markér hele tracket |
| Ctrl+Z / Ctrl+Y (eller Ctrl+Shift+Z) | Fortryd / gentag |
| Ctrl+↑/↓ (+Shift = oktav) | Transponér |
| Num \* / F2 | Oktav op |
| Num / / F1 | Oktav ned |
| Ctrl+N / O / S / Shift+S | Nyt, åbn, gem, gem som |
| Ctrl+I / Ctrl+E | Åbn lydfil som instrument / eksportér lyd |
| Ctrl+T / M / P / Q | Nyt track, mixer, plug-in manager, afslut |

---

## 16. Grænser og fejlfinding

| | Grænse |
|---|---|
| Tracks | 64 |
| Note-kolonner pr. track | 8 |
| Linjer pr. pattern | 512 |
| Toner | `C-0` – `B-9` (0–119) |
| BPM / LPB | 20–999 / 1–32 |
| Eksportlængde | 30 minutter |

- **Ingen lyd:** Tjek lydkortet under *Options → Audio & MIDI Settings…*, at master-stripen ikke er mutet,
  og at der er valgt et instrument. Står der *SoundFont missing*, skal du vælge en `.sf2` under
  *Options → Default SoundFont…*.
- **Lyden hakker:** Hæv buffer size i lydindstillingerne, og luk tunge plug-ins.
- **Et plug-in mangler, når et projekt åbnes:** Programmet viser en liste over de plug-ins og filer, der
  ikke kunne indlæses. Scan igen i Plug-in Manager, og åbn projektet på ny.

---

## Appendiks A – Filformatet `.artixml`

En `.artixml`-fil er et almindeligt **UTF-8 XML-dokument**. Det kan læses og redigeres i enhver teksteditor
eller behandles med XML-værktøjer. Rodelementet hedder `ARTILODY_PROJECT`.

### A.1 Overordnet struktur

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ARTILODY_PROJECT version="1.0.3">
  <SONG title="…" bpm="128.0" lpb="4" sequence="0,0,1">
    <TRACKS>
      <TRACK … />            <!-- ét pr. track, i rækkefølge -->
    </TRACKS>
    <PATTERNS>
      <PATTERN …>            <!-- ét pr. pattern; nummeret er positionen (0, 1, 2 …) -->
        <T d="…"/>           <!-- ét pr. track, samme rækkefølge som TRACKS -->
      </PATTERN>
    </PATTERNS>
  </SONG>
  <STRIPS>
    <STRIP …> … </STRIP>     <!-- én channel strip pr. track, samme rækkefølge -->
  </STRIPS>
  <STRIP master="1" …> … </STRIP>   <!-- master-stripen -->
</ARTILODY_PROJECT>
```

Tal skrives **decimalt** i filen, også dem der vises i hex i programmet. Farver skrives som 8-cifret
hex ARGB, fx `ff2f7bf0` (alfa `ff`, rød `2f`, grøn `7b`, blå `f0`). Boolske værdier skrives som `1`/`0`.

### A.2 `ARTILODY_PROJECT`

| Attribut | Betydning |
|---|---|
| `version` | Den programversion, der gemte filen. |

### A.3 `SONG`

| Attribut | Type | Betydning |
|---|---|---|
| `title` | tekst | Sangens titel |
| `bpm` | decimaltal | Starttempo (20–999) |
| `lpb` | heltal | Linjer pr. slag (1–32) |
| `sequence` | kommasepareret liste | Sekvensen: pattern-numre (0-baseret) i afspilningsrækkefølge, fx `0,0,1,2` |

### A.4 `TRACK` (i `TRACKS`)

| Attribut | Type | Betydning |
|---|---|---|
| `name` | tekst | Trackets navn |
| `colour` | ARGB-hex | Trackets farve |
| `columns` | 1–8 | Antal note-kolonner |
| `uid` | heltal | Internt, unikt track-id |

Track nummer *n* (1-baseret) er det *n*'te `TRACK`-element. Det samme nummer bruges i Ins-kolonnen.

### A.5 `PATTERN` (i `PATTERNS`)

| Attribut | Type | Betydning |
|---|---|---|
| `name` | tekst | Patternets navn |
| `colour` | ARGB-hex | Farve |
| `rows` | 1–512 | Antal linjer |
| `notes` | tekst | Patternets noter |

Hvert `PATTERN` har ét `T`-element pr. track. Attributten `d` indeholder trackets **ikke-tomme celler**
adskilt af mellemrum. Hver celle er syv kommaseparerede heltal:

```
række,kolonne,node,ins,vol,fxKommando,fxVærdi
```

| Felt | Område | Betydning |
|---|---|---|
| `række` | 0 – rows−1 | Linjenummer |
| `kolonne` | 0–7 | Note-kolonne i tracket |
| `node` | 0–119, −1, −2 | MIDI-tonenummer (60 = `C-5`, 48 = `C-4`). **−1** = tom, **−2** = note off (`OFF`) |
| `ins` | 1–254, 255 | Track-nummer, hvis instrument spiller tonen. **255** = tom (trackets eget instrument) |
| `vol` | 0–127, 255 | Anslag/lydstyrke. **255** = tom |
| `fxKommando` | 0 eller ASCII-kode | Effektbogstavets tegnkode: `T`=84, `L`=76, `D`=68, `C`=67, `R`=82, `P`=80, `M`=77, `B`=66. **0** = ingen effekt |
| `fxVærdi` | 0–255 | Effektens værdi (i programmet vist som hex `xx`) |

Celler, der ikke står i listen, er tomme. `d=""` betyder, at tracket er tomt i dette pattern.

**Eksempel:** `0,0,60,3,72,0,0 10,1,64,3,56,68,128 14,0,-2,255,255,0,0` betyder:

- Linje 0, kolonne 0: `C-5`, instrument 03, vol 48 (hex), ingen effekt.
- Linje 10, kolonne 1: `E-5`, instrument 03, vol 38, effekt `D80` (68 = `D`, 128 = `80` hex).
- Linje 14, kolonne 0: `OFF`.

### A.6 `STRIP` (channel strip)

`STRIPS` indeholder én `STRIP` pr. track i samme rækkefølge som `TRACKS`. Master-stripen ligger direkte under
`ARTILODY_PROJECT` med `master="1"`.

| Attribut | Type | Betydning |
|---|---|---|
| `volume` | dB | Fader, −60 til +6 |
| `pan` | −1 – 1 | Panorering (−1 = venstre, 0 = midt, 1 = højre) |
| `mute` / `solo` | 0/1 | Mute og solo |
| `fxCollapsed` | 0/1 | Om plug-in-listen er kollapset |
| `master` | 1 | Kun på master-stripen |

En `STRIP` indeholder tre underelementer: `INSTRUMENT`, `FXCHAIN` og `BUILTINS`.

#### `INSTRUMENT`

Attributten `kind` bestemmer typen:

| `kind` | Øvrige attributter |
|---|---|
| `soundfont` | `genre` (Pop/Rock/Electro/Jazz eller tom), `name` (preset-navn), `bank` (128 = trommesæt), `program` (0–127), `drums` (0/1), `file` (kun hvis en anden `.sf2` end standarden er brugt: fuld sti) |
| `sample` | `file` (fuld sti til lydfilen), `root` (grundtone, standard 60) |
| `plugin` | Indeholder ét `PLUGIN`-element (se nedenfor) |
| `none` | Intet instrument (altid på master-stripen) |

#### `FXCHAIN`

Indeholder ét `PLUGIN`-element pr. plug-in-effekt i signalrækkefølge. Hvert element har desuden attributten
`bypass` (0/1).

#### `PLUGIN`

```xml
<PLUGIN state="…base64…" bypass="0">
  <PLUGIN name="…" format="VST3" manufacturer="…" file="…" uniqueId="…" isInstrument="0" … />
</PLUGIN>
```

- Attributten `state` er plug-in'ets egen tilstand (dets "chunk"), base64-kodet. Indholdet bestemmes af
  plug-in'et.
- Det indre `PLUGIN`-element er JUCE's *PluginDescription*. Det bruges til at finde og genindlæse
  plug-in'et: navn, format (`VST3`, `AudioUnit`, `CLAP`), producent, fil/identifier, unikt id osv.

#### `BUILTINS`

Indeholder tre `FX`-elementer for de indbyggede effekter. Hvert har `name`, `enabled` (0/1) og en attribut
pr. parameter:

| `name` | Parametre (attributnavn = værdi) |
|---|---|
| `EQ` | `lowcut`, `lowfreq`, `lowgain`, `midfreq`, `midgain`, `midq`, `hifreq`, `higain` (Hz / dB / Q) |
| `Compressor` | `threshold` (dB), `ratio`, `attack` (ms), `release` (ms), `knee` (dB), `makeup` (dB) |
| `Reverb` | `size`, `damping`, `width`, `mix` (0–1) |

Områder og standardværdier står i afsnit 12. Mangler en parameter, bruges standardværdien.

### A.7 Robusthed og redigering i hånden

Programmet tåler mange afvigelser i filen:

- Ukendte elementer og attributter ignoreres.
- Manglende værdier får standardværdier.
- Sekvensnumre, kolonner og rækkeantal begrænses til gyldige områder.
- Celler uden for patternet springes over.
- Plug-ins eller filer, der ikke kan findes, rapporteres, når projektet åbnes. Resten af projektet
  indlæses alligevel.

Hvis du redigerer en fil i hånden, skal antallet af `TRACK`, `T` (pr. pattern) og `STRIP` (i `STRIPS`)
stemme overens.

### A.8 Komplet minimalt eksempel

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ARTILODY_PROJECT version="1.0.3">
  <SONG title="Mini" bpm="120.0" lpb="4" sequence="0">
    <TRACKS>
      <TRACK name="Piano" colour="ff2f7bf0" columns="1" uid="1"/>
    </TRACKS>
    <PATTERNS>
      <PATTERN name="Init" colour="ffff5a4e" rows="16" notes="">
        <T d="0,0,60,1,100,0,0 4,0,64,1,100,0,0 8,0,67,1,100,0,0 12,0,-2,255,255,0,0"/>
      </PATTERN>
    </PATTERNS>
  </SONG>
  <STRIPS>
    <STRIP volume="0.0" pan="0.0" mute="0" solo="0" fxCollapsed="0">
      <INSTRUMENT kind="soundfont" genre="Pop" name="Grand Piano" bank="0" program="0" drums="0"/>
      <FXCHAIN/>
      <BUILTINS/>
    </STRIP>
  </STRIPS>
  <STRIP volume="0.0" pan="0.0" mute="0" solo="0" fxCollapsed="0" master="1">
    <INSTRUMENT kind="none"/>
    <FXCHAIN/>
    <BUILTINS/>
  </STRIP>
</ARTILODY_PROJECT>
```

Eksemplet spiller C-dur-akkordens toner (C-5, E-5, G-5) som fjerdedele på et klaver og slipper tonen på linje 12.
