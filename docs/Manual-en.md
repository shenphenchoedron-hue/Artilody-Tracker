# Artilody Tracker – User Manual

*Version 1.0.7 · [Dansk version](Manual-da.md)*

Artilody Tracker is a pattern-based music tracker. Music is written as notes in vertical columns that are
played line by line from top to bottom. Unlike most trackers, every track has its own **channel strip**
directly below it, with instrument, effects, EQ, compressor, reverb, panning and volume.

---

## Contents

1. [Installation and first start](#1-installation-and-first-start)
2. [Basic concepts](#2-basic-concepts)
3. [Window layout](#3-window-layout)
4. [Top bar and transport](#4-top-bar-and-transport)
5. [Tracks](#5-tracks)
6. [The pattern editor](#6-the-pattern-editor)
7. [Effect commands](#7-effect-commands)
8. [Sequence and patterns](#8-sequence-and-patterns)
9. [The mixer and channel strips](#9-the-mixer-and-channel-strips)
10. [Instruments](#10-instruments)
11. [Plug-ins (VST3, AU, CLAP)](#11-plug-ins-vst3-au-clap)
12. [Built-in effects](#12-built-in-effects)
13. [Projects, export and settings](#13-projects-export-and-settings)
14. [Menus](#14-menus)
15. [Keyboard shortcuts](#15-keyboard-shortcuts)
16. [Limits and troubleshooting](#16-limits-and-troubleshooting)
- [Appendix A – The `.artixml` file format](#appendix-a--the-artixml-file-format)

---

## 1. Installation and first start

Download the latest version from *Releases* on GitHub:

| Platform | File |
|---|---|
| Windows 10/11 (64-bit) | `ArtilodyTracker-*-Windows-x64-Setup.exe` or `*-portable.zip` |
| macOS 11+ (Apple Silicon and Intel) | `ArtilodyTracker-*-macOS-universal.dmg` |
| Debian 12 / Ubuntu 22.04 / 24.04 | `artilody-tracker_*_<distro>_amd64.deb` |

- **macOS:** Drag the app to *Applications*. The app is not notarised, so the first time you must
  right-click it and choose *Open* (or run `xattr -dr com.apple.quarantine "/Applications/Artilody Tracker.app"`).
- **Windows:** SmartScreen may warn about an unknown publisher – choose *More info → Run anyway*.
- **Debian/Ubuntu:** `sudo apt install ./artilody-tracker_*_amd64.deb`

On first start, the demo song *Sunrise Circuits* is loaded. Press **Space** to hear it.

The bundled *GeneralUser GS* SoundFont provides the built-in instruments. If it is missing, choose another
`.sf2` file under *Options → Default SoundFont…*.

Choose your audio device under *Options → Audio & MIDI Settings…* (or click *Main Out* on the master strip).
If the sound crackles or stutters, increase the **buffer size** (for example 512 or 1024 samples).

---

## 2. Basic concepts

| Term | Meaning |
|---|---|
| **Track** | A vertical strip in the editor. Every track has its own channel strip and its own instrument. |
| **Note column** | A track can have 1–8 note columns, so it can play chords. Each column plays one note at a time. |
| **Line (row)** | A horizontal row. Lines are played one at a time from top to bottom. |
| **Pattern** | A block of music of 1–512 lines covering all tracks. |
| **Sequence** | The order of patterns in the song (order list). The same pattern can be used several times. |
| **BPM** | Tempo in beats per minute. |
| **LPB** | *Lines per beat* – how many lines make up one beat. At LPB 4, each line is a 1/16 note. |
| **Edit mode** | With edit mode on you can write into the pattern: click a note field to choose a note, or play a MIDI keyboard. |

All numbers in patterns (instrument, volume and effect values) are shown in **hexadecimal** (00–FF), as is
customary in trackers.

---

## 3. Window layout

```
┌──────────────────────────── Top bar (transport, tempo, meter, project) ─────────────────────────────┐
├─ + Add Track │ Columns − + │ Pattern info ───────────────────────── Collapse FX lists │ Hide Mixer ─┤
├──────┬─────┬──────────────────────────────────────────────────────────────┬──────────────────────────┤
│ Seq  │ Row │  Track tabs   01 Drums │ 02 Bass │ 03 Chords │ +              │ Patterns                 │
│      │ no. │  Note Ins Vol FX ...                                         │                          │
│ 00 ▶ │ 00  │  C-4  01  7F ---                                             │ Pattern Properties       │
│ 01   │ 01  │  ---  --  -- ---   (pattern grid)                            │  Name, Length,           │
│ ...  │ ... │                                                              │  Colour, Notes           │
│      │     ├──────────────────────────────────────────────────────────────┤                          │
│Master│     │  One channel strip per track (directly below the track)      │ Keys (help)              │
└──────┴─────┴──────────────────────────────────────────────────────────────┴──────────────────────────┘
```

- **Top bar** (top): transport, tempo, octave, edit step, master meter, CPU, undo/redo and project name.
- **Toolbar**:
  - **+ Add Track**
  - **Columns − / +**: number of note columns on the selected track
  - **Edit: On / Off**: turns edit mode on and off (red when on)
  - **Loop: On / Off**: turns loop on and off (green when on)
  - an info box with the pattern's number and name, plus the cursor's track and line
  - **Collapse FX lists** and **Hide Mixer / Show Mixer**
- **Left column**: the sequence list (*Seq*), line numbers (hex) and, at the bottom, the **master strip**.
- **Centre**: track tabs, the pattern grid and one channel strip below each track. The area scrolls
  horizontally when there are many tracks.
- **Right panel**: the pattern list, pattern properties and a short keyboard help.

When **edit mode** is on, the grid gets a red frame and the cursor turns red.

---

## 4. Top bar and transport

| Button / field | Function |
|---|---|
| **Rewind** | Goes to the start of the song (sequence slot 0, line 0) without starting playback. |
| **Play** | Plays the song (**Space**). After **Stop** it continues from where it stopped. If you have selected another sequence slot in the meantime, it starts there. |
| **Loop** (circular arrow) | Turns loop on/off, like **Loop: On / Off** in the toolbar. See below. |
| **Stop** | Stops and silences all notes. The position is remembered, so **Play** continues from there. |
| **Record** | Toggles edit mode (**Esc**). |
| **BPM** | Tempo, 20–999. Shows the current tempo during playback, including changes made by `Txx` effects. |
| **LPB** | Lines per beat, 1–32. |
| **Oct** | Default octave for the *Choose note* window, 0–9 (default 4). |
| **Step** | Edit step, 0–16: how many lines the cursor moves after you enter a value. 0 = it stays put. |
| **Follow** | When on, the grid follows playback. |
| **MASTER / CPU** | Level meter for the master output, and the audio engine's CPU load. |
| **Undo / Redo** | Undo / redo (**Ctrl+Z / Ctrl+Y**). |
| **Project** | The song title. It shows *(modified)* when there are unsaved changes. |

The **number fields** (BPM, LPB, Oct, Step) can be changed in four ways:

- **Drag** up or down with the mouse.
- **Scroll** with the mouse wheel (±1).
- **Click** once to pick a preset value from a menu.
- **Double-click** to type an exact value.

**Loop:** The **Loop: On / Off** button in the toolbar (and the circular arrow in the top bar) turns loop on
and off. The button is green when loop is on.

- With loop on, **Play** repeats the current pattern instead of playing the whole song.
- To loop only some rows, drag with the mouse over the **row numbers** to the left of the grid. The rows are
  marked with a green band, and the loop button shows them, for example *Loop: On 04-0B*.
- Rows off screen: drag past the top or bottom row and the grid scrolls along. Or click the first row, scroll
  down (with the mouse wheel over the grid or the row numbers) and **Shift+click** the last row.
- You can turn loop on and off and mark new rows while the music plays. It switches straight away.
- Double-click the row numbers (or right-click → *Clear loop rows*) to remove the marking, so the whole
  pattern loops again.
- **Shift+Space** turns loop on and starts playback.

**Playback from the keyboard:**

- **Space**: play / stop.
- **Shift+Space**: loop pattern.
- **Enter**: play from the line the cursor is on.

---

## 5. Tracks

**Create a track** with any of these:

- **+ Add Track**
- **+** to the right of the last tab
- **Ctrl+T**
- *Song → Add Track*

The new track gets the instrument *Pop / Grand Piano*. You can have up to 64 tracks.

**Tab:**

- **Click** selects the track.
- **Double-click** renames it (up to 24 characters).
- **Right-click** or the **…** button opens the track menu.

**Track menu:**

| Item | Function |
|---|---|
| Rename… | Rename the track. |
| Colour | Choose one of 10 colours. |
| Note columns | 1–8 note columns (also via **Columns − / +** in the toolbar). |
| Choose instrument… | Opens the instrument menu (see section 10). |
| Add plug-in effect… | Add a plug-in effect. |
| Move left / Move right | Move the track. Ins numbers in all patterns are updated automatically. |
| Insert track after | Insert an empty track (no instrument) after this one. |
| Delete track | Delete the track and its channel strip (asks for confirmation). |

> Moving, inserting and deleting tracks clears the undo history.

---

## 6. The pattern editor

### The columns

Each note column has four fields:

```
C-4 01 7F D80
│   │  │  └─ FX:   effect command (letter) + value (hex)
│   │  └──── Vol:  volume/velocity 00–7F
│   └─────── Ins:  instrument (track number, hex)
└─────────── Note: note, OFF (note off) or --- (empty)
```

- **Note** is shown as name and octave, for example `C-4`, `F#5`. Note 60 (MIDI middle C) is shown as `C-5`.
- **Ins** selects which track's instrument plays the note: `01` = track 1, `0A` = track 10.
  - If the field is empty (or `00`/invalid), the note plays on the track's own instrument.
  - The sound goes through the chosen instrument's channel strip.
  - When you enter a note into an empty Ins field, it is filled in with the track's own number.
- **Vol** is the velocity. `7F` is full velocity; an empty field equals about `66` (80 %).
- **FX** is an effect command (see section 7).

Lines that start a beat (every LPB-th line) have a lighter background and a bold line number.

### Entering notes

1. Turn on edit mode with the **Edit** button in the toolbar or with **Esc**. The grid gets a red frame.
2. Click a note field. A small window, **Choose note**, opens with octave buttons (1–8) and a one-octave
   keyboard:
   - Click a key to select and hear the note. The top-right corner shows the note, for example `C-3`.
   - Click an octave button to choose the octave.
   - **Insert note** (or **Return**) writes the note. Double-clicking a key writes it straight away.
   - **Note off** writes **OFF**: the previous note in the column is released.
   - **Cancel**, the cross or **Esc** closes the window without writing anything.
   - The keyboard works in the window too: **←/→** chooses the note, **↑/↓** the octave.

The window starts in the octave the field already has, otherwise in the default octave (*Oct* in the top
bar, which also follows the last octave you picked). After a note is written, the cursor moves *Step* lines
down.

The computer keyboard is no longer used to play notes, because its keys don't match the note names.

**MIDI keyboard:** All MIDI inputs are active, including keyboards connected while the program is running.
Notes always play on the cursor's track. In edit mode they are written at the cursor with the velocity as
the Vol value, and the cursor moves *Step* lines down. If you hold several keys at once, the chord is
written into the next note columns on the same line, if the track has more columns.

### Instrument, volume and effect with the mouse

In edit mode, clicking the other fields also opens a small window that fits the field:

| Field | Window |
|---|---|
| **Ins** | **Choose instrument**: a list of all tracks with colour, name and instrument. Click to select; double-click or **Use instrument** to write it. **Clear** empties the field so the note plays on the track's own instrument. |
| **Vol** | **Volume**: a volume slider (00–7F) with a large bar showing the value in hex, percent and dB. **−/+** and the arrow keys change it in steps of 1 (Shift = 16), and the *pp, p, mf, f, ff* buttons give fixed levels. **Clear** gives the default velocity. |
| **FX** | **Effect**: choose a command (Tempo, Lines/beat, Delay, Cut, Retrigger, Pitch bend, Modulation, Jump). Below the buttons it says what the effect does and what the value means, for example *120 BPM* or *Delay by 50 % of a line*. The value is set with a slider in a range that suits the command. Retrigger has two sliders: how often the note repeats and how much its volume falls. **Clear** removes the effect. |

The top-right corner of the window shows the value that will be written, for example `03`, `50` or `T78`.
**Return** writes it, **Esc** closes. After writing, the cursor moves *Step* lines down.

### Hex values

You can also type values directly. Put the cursor in the Ins, Vol or FX value field and type **0–9** and
**A–F**:

- Each field has two digits (high and low). The cursor moves down after each digit.
- In the FX command field you type a letter, for example **T** or **D**.
- **Del** clears the field: in the note field the whole cell, otherwise only the field under the cursor.

### Navigation and selection

| Key | Function |
|---|---|
| ↑ / ↓ | One line up/down (wraps from bottom to top) |
| PgUp / PgDn | 4 beats (16 lines at LPB 4) |
| Home / End | First / last line |
| ← / → | Previous / next field, continuing into the next column and track |
| Tab / Shift+Tab | Next / previous track |
| Ctrl+← / Ctrl+→ | Previous / next track |
| Shift + arrow keys | Select. Shift+←/→ selects a whole note column at a time |
| Mouse wheel | Scrolls 2 lines. Shift+wheel scrolls horizontally between tracks |
| Click / Shift+click / drag | Place the cursor / extend the selection / select an area |

### Editing

| Key | Function |
|---|---|
| Ctrl+C / X / V | Copy / cut / paste (selection or single cell). Paste starts at the cursor. |
| Ctrl+A | Select the whole track |
| Ctrl+↑ / ↓ | Transpose ±1 semitone |
| Ctrl+Shift+↑ / ↓ | Transpose ±1 octave |
| Del | Clear the selection or field |
| Ins | Insert an empty line in the current note column (edit mode) |
| Backspace | Delete the line in the current note column and pull the rest up (edit mode) |
| Ctrl+Z / Ctrl+Y | Undo / redo |

Right-clicking in the grid gives the same commands. You can also choose **Interpolate volume** (a linear
ramp of Vol between the first and last line of the selection) and view a list of **Effect commands**.

---

## 7. Effect commands

Effects are written in the FX column as one letter followed by a hex value `xx`.

| Command | Function | Example |
|---|---|---|
| `Txx` | Set tempo to `xx` BPM (hex, 20–FF = 32–255 BPM) | `T78` = 120 BPM |
| `Lxx` | Set lines per beat (01–20 hex = 1–32) | `L08` |
| `Dxx` | Delay the note by `xx`/256 of a line | `D80` = half a line (swing) |
| `Cxx` | Cut the note after `xx`/256 of a line | `C40` = staccato |
| `Rxy` | Retrigger the note every `y` ticks (12 ticks per line). `x` = how much the volume falls per retrigger (x/16) | `R03` = 4 hits per line |
| `Pxx` | Pitch bend: 00 = down, 80 = centre, FF = up | `P80` |
| `Mxx` | Modulation wheel (MIDI CC1), 00–7F | `M40` |
| `Bxx` | Jump to sequence slot `xx` after this line (not while looping a pattern) | `B00` |

Tempo and LPB return to the song's values when playback is stopped and started again.

---

## 8. Sequence and patterns

### The sequence list (*Seq*, left)

Each slot shows its number and the number of the pattern it plays, in the pattern's colour.

- **Click** a slot to show its pattern.
- **+** inserts a new pattern after the selected slot.

**Right-click menu:**

| Item | Function |
|---|---|
| Insert new pattern after | New empty pattern after the slot |
| Repeat this pattern after | The same pattern once more |
| Duplicate pattern into new slot | A copy of the pattern in a new slot |
| Use pattern | Choose which pattern the slot plays |
| Move up / Move down | Move the slot |
| Remove slot | Remove the slot |
| Play from here | Play the song from this slot |

### The pattern list (right panel)

- **Click** selects a pattern.
- **Double-click** puts it into the selected sequence slot.
- **+** creates a new pattern without adding it to the sequence.

**The … menu:**

- *Duplicate pattern*
- *Clear pattern*
- *Use in selected sequence slot*
- *Append to end of sequence*
- *Delete pattern*: also removes every sequence slot that uses the pattern.

### Pattern Properties

| Field | Function |
|---|---|
| **Name** | The pattern's name. |
| **Length** | 8–512 lines. Making a pattern shorter deletes the lines past the new end and clears the undo history. |
| **Colour** | Colour in the sequence and pattern lists. |
| **Notes** | Free text, for example chords or ideas. |

---

## 9. The mixer and channel strips

Every track has a **channel strip** below it. Sound runs through the strip in this order:

```
Instrument → plug-in effects (in order) → EQ → Compressor → Reverb → Pan / Volume → Master
```

From the top, the strip contains:

1. **Header** with track number, name and **…** menu (the same menu as the track tab).
2. **Instrument button**, showing the instrument's name and type.
   - **Click** opens the instrument menu.
   - **Double-click** or **Alt+click** opens a plug-in instrument's editor.
3. **PLUG-INS**: the list of plug-in effects, with **+** (add) and an arrow (collapse the list).
   - **Click** an effect to open its editor.
   - The power icon bypasses the effect.
   - ▲ ▼ ✕ move up, move down and remove (shown when the mouse is over the row). The same actions are in
     the right-click menu.
   - Below the list are the built-in effects **EQ**, **Comp** and **Reverb** (see section 12).
4. **Fader** (−60 to +6 dB) with a stereo meter. Double-click resets it to 0 dB.
5. **Pan knob** (L–C–R). Double-click centres it.
6. **M** (mute) and **S** (solo). If any track is soloed, only the soloed tracks are heard.

### The master strip

The master strip at the bottom left receives the sum of all tracks:

- It has plug-in effects, EQ, compressor, reverb, volume, pan and mute, but no solo.
- Its instrument button shows **Main Out** and the audio device. Clicking it opens the audio settings.
- Its menu has *Reset master strip* and *Audio settings…*.

### Hiding the mixer

**Hide Mixer**, **Ctrl+M** or the arrow above the master strip shrinks all strips. Only the name, M/S, a
horizontal fader and a meter remain, which leaves more room for patterns.

**Collapse FX lists** collapses or expands the plug-in lists in all strips at once.

---

## 10. Instruments

Click the instrument button on a channel strip to open the instrument menu.

- **Pop / Rock / Electro / Jazz**: curated instruments from the built-in SoundFont, for example drum kits
  (Pop, Room, Rock, Electronic, 808, Jazz, Brush), piano, Rhodes, bass, guitars, synths, pads, brass and
  vibraphone.
- **All presets: …**: every preset in the default SoundFont, sorted by bank. Drum kits are in bank 128.
- **Load SoundFont file (.sf2)…**: use your own SoundFont. The first preset in the file is selected.
- **Load audio file as sampler…**: use an audio file (WAV, AIFF, FLAC, OGG, MP3) as an instrument.
  - The sample plays chromatically with root note `C-5`, i.e. note 60: the **Q** key at octave 4.
  - The sampler has 24 voices, and audio files up to 120 seconds long are loaded.
  - Same function: *File → Open Audio File as Instrument…* (**Ctrl+I**).
- **Instrument plug-ins**: VST3, AU (macOS) and CLAP instruments, sorted by vendor. The editor opens
  automatically.
- **Open instrument editor** / **Remove instrument**.

> Audio files and custom SoundFonts are saved in the project as **paths**, not as the files themselves. If
> you move the files, you must choose them again.

---

## 11. Plug-ins (VST3, AU, CLAP)

Open the **Plug-in Manager** with **Ctrl+P** or *Plug-ins → Plug-in Manager…*, and scan for plug-ins:

- **Scan for new plug-ins** at the top of the window scans VST3, AU and CLAP in one go. So does
  *Plug-ins → Scan for New Plug-ins (all formats)*.
  - Only new or updated plug-ins are loaded, so a repeat scan is quick.
  - The window doesn't respond to clicks while a scan runs.
- The list shows name, format (VST3/AU/CLAP), type, vendor and version. Plug-ins that crashed during
  scanning are marked **FAILED** and skipped next time. Right-click a plug-in to remove it or show its folder.
- Under **Options…** you can scan one format at a time and change which folders are scanned. Those folders
  are also used by *Scan for new plug-ins*.

- **VST3** on all platforms, **Audio Unit** on macOS and **CLAP** on all platforms.
- CLAP plug-ins are searched for in the standard folders and in folders listed in the `CLAP_PATH`
  environment variable:
  - macOS: `/Library/Audio/Plug-Ins/CLAP`, `~/Library/Audio/Plug-Ins/CLAP`
  - Windows: `%COMMONPROGRAMFILES%\CLAP`
  - Linux: `~/.clap`, `/usr/lib/clap`
- If a plug-in crashes the program during a scan, it is skipped on the next scan.

You then pick plug-ins from the instrument menu or with **+** in the PLUG-INS list. Each plug-in's state is
saved in the project. Plug-ins without their own editor get a simple parameter editor.

---

## 12. Built-in effects

Click **EQ**, **Comp** or **Reverb** on a strip to open the effect's editor. The effect turns on
automatically when you change something.

### Parametric EQ

The EQ is a parametric EQ with up to **8 freely placed bands**. Each band is an IIR biquad filter. The graph
shows frequency (20 Hz–20 kHz) horizontally and gain (±24 dB) vertically. The thick line is the total
response, and each band has its own numbered point and colour.

| Action | Function |
|---|---|
| **Click on the line** | Inserts a point at that frequency. It starts at 0 dB, so the sound doesn't change until you drag it. Keep the button down to drag it straight away. |
| **Double-click** in the graph | Inserts a point at the mouse. |
| **Drag** a point | Horizontal = frequency, vertical = gain. |
| **Shift + drag** | Fine adjustment (10 times slower) to hit a value precisely. |
| **Mouse wheel** over a point | Changes Q (bandwidth). With Shift in small steps. |
| **Double-click** a point | Deletes the band. |
| **Right-click** a point | Choose the type, reset gain or Q, or delete the band. |
| **Reset** | Removes all bands. |

The panel below the graph shows the selected band: type, **Frequency**, **Gain** and **Q / Width**. Click the
number below a knob to type an exact value, for example `1.25k`. Double-clicking a knob resets it.

| Type | Effect | Vertical drag |
|---|---|---|
| Bell | Boosts or cuts around the frequency | Gain |
| Low Shelf / High Shelf | Boosts or cuts everything below / above the frequency | Gain |
| Low Cut / High Cut | Removes everything below / above the frequency (12 dB/octave) | Q (resonance) |
| Notch | Removes a narrow frequency completely | Q (width) |

| Parameter | Range |
|---|---|
| Frequency | 20 Hz – 20 kHz |
| Gain | −24 – +24 dB |
| Q | 0.1 – 18 |

When a point is moved during playback, the filter glides smoothly to the new setting (about 25 ms), so it
does not click.

### Compressor and Reverb

- Double-clicking a knob resets it.
- **Reset** sets all knobs back to their defaults.

**Compressor** has a gain-reduction meter:

| Parameter | Range | Default |
|---|---|---|
| Threshold | −60–0 dB | −18 dB |
| Ratio | 1:1–20:1 | 4:1 |
| Attack | 0.1–200 ms | 10 ms |
| Release | 10–1500 ms | 150 ms |
| Knee | 0–24 dB | 6 dB |
| Makeup | 0–24 dB | 0 dB |

**Reverb:**

| Parameter | Range | Default |
|---|---|---|
| Size | 0–1 | 0.55 |
| Damping | 0–1 | 0.45 |
| Width | 0–1 | 1.00 |
| Mix | 0–1 | 0.25 |

---

## 13. Projects, export and settings

- **Save**: *File → Save Project* (**Ctrl+S**) / *Save Project As…* (**Ctrl+Shift+S**).
  - Projects are saved as **`.artixml`**, a plain XML file (see Appendix A).
  - The file contains the song, all patterns, all channel strips and plug-in states.
- **Open**: *File → Open Project…* (**Ctrl+O**) or *Open Recent Project*. On macOS and Windows you can also
  double-click an `.artixml` file.
- **Older projects**: Files from version 1.0.0–1.0.2 use the `.artilody` extension and can still be
  opened. When you save, a new `.artixml` file is written next to the old one. The old file is left
  untouched.
- **Export**: *File → Export Song as Audio…* (**Ctrl+E**) saves the whole song as **WAV, AIFF or FLAC**.
  - The audio is 24-bit at the audio device's sample rate.
  - The sequence plays once, plus a 2.5-second tail.
  - The export can be cancelled while it runs.
- **MIDI**: *File → Export Song as MIDI File…* saves the song as a Standard MIDI File (`.mid`, format 1) for
  other music programs.
  - There is one MIDI track per instrument (track), with the track's name and a program change from the
    SoundFont preset. Drum kits play on channel 10.
  - The song plays as it does in the program: sequence order, `Bxx` jumps, tempo (`Txx`), `Lxx`, delay
    (`Dxx`), cut (`Cxx`), retrigger (`Rxy`), note off and the Ins column. Pitch bend (`Pxx`) and modulation
    (`Mxx`) are included as MIDI messages. Vol becomes velocity.
  - Instruments from audio files and plug-ins get program 1 (piano), which you can change in the other
    program.
- **MusicXML**: *File → Export Song as MusicXML…* saves the song as sheet music (`.musicxml`, MusicXML 4.0)
  that opens in, for example, MuseScore, Dorico, Finale or Sibelius.
  - One part per instrument with a treble or bass clef depending on pitch; drums get a percussion clef.
  - 4/4 time. Notes are rounded to the nearest 1/32 note, and long notes are tied across bar lines.
  - Notes with the same start and length are written as chords; overlapping notes are spread over up to
    four voices.
  - Tempo and tempo changes are written as metronome marks. A retrigger is written as one note.
- **Demo songs**: *File → Demo Songs* has *Sunrise Circuits* (pop/electro) and *Blue Corner Swing* (jazz).
- **New project** (**Ctrl+N**) creates four tracks:
  - Drums (Pop Drum Kit)
  - Bass (Finger Bass)
  - Chords (Grand Piano with 3 columns)
  - Lead (Square Lead)

**Settings** are saved automatically in the file `Artilody Tracker.settings`:

| System | Location |
|---|---|
| macOS | `~/Library/Application Support/Artilody Tracker/` |
| Windows | `%APPDATA%\Artilody Tracker\` |
| Linux | `~/.config/artilody-tracker/` |

The file holds the audio device, recent projects, last folder, default SoundFont, mixer state and the list
of scanned plug-ins.

---

## 14. Menus

| Menu | Contents |
|---|---|
| **File** | New Project, Open Project…, Open Recent Project, Save Project, Save Project As…, Open Audio File as Instrument…, Export Song as Audio…, Export Song as MIDI File…, Export Song as MusicXML…, Demo Songs, Quit |
| **Edit** | Undo, Redo, Cut, Copy, Paste, Delete, Select Track Column, Transpose ±1 / ±12, Interpolate Volume, Edit Mode |
| **Song** | Play Song, Play From Start, Go to Start, Loop Pattern, Stop, Follow Playback, Add Track, Rename Song… |
| **View** | Show Mixer, Collapse / Expand All Plug-in Lists |
| **Plug-ins** | Plug-in Manager (scan VST3 / AU / CLAP)…, Scan for New Plug-ins (all formats) |
| **Options** | Audio & MIDI Settings…, Default SoundFont… |
| **Help** | Keyboard Shortcuts, About Artilody Tracker |

---

## 15. Keyboard shortcuts

On macOS, use **Cmd** instead of **Ctrl**.

| Key | Function |
|---|---|
| Esc | Edit mode on/off |
| Space / Shift+Space | Play (continue) song / loop pattern or loop rows (stop if playing) |
| Enter | Play from the cursor's line |
| Click a field (edit mode) | Window for note, instrument, volume or effect |
| MIDI keyboard (edit mode) | Enter notes (several keys = chord) |
| 0–9, A–F | Hex values in Ins / Vol / FX |
| Arrow keys, Tab, PgUp/PgDn, Home/End | Navigation |
| Shift + arrow keys / mouse drag | Selection |
| Del / Ins / Backspace | Clear, insert line, delete line |
| Ctrl+C / X / V | Copy, cut, paste |
| Ctrl+A | Select the whole track |
| Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z) | Undo / redo |
| Ctrl+↑/↓ (+Shift = octave) | Transpose |
| Num \* / F2 | Default octave up |
| Num / / F1 | Default octave down |
| Ctrl+N / O / S / Shift+S | New, open, save, save as |
| Ctrl+I / Ctrl+E | Open audio file as instrument / export audio |
| Ctrl+T / M / P / Q | New track, mixer, plug-in manager, quit |

---

## 16. Limits and troubleshooting

| | Limit |
|---|---|
| Tracks | 64 |
| Note columns per track | 8 |
| Lines per pattern | 512 |
| Notes | `C-0` – `B-9` (0–119) |
| BPM / LPB | 20–999 / 1–32 |
| Export length | 30 minutes |

- **No sound:** Check the audio device under *Options → Audio & MIDI Settings…*, that the master strip is
  not muted, and that an instrument is selected. If you see *SoundFont missing*, choose an `.sf2` under
  *Options → Default SoundFont…*.
- **Crackling or stuttering sound:** Increase the buffer size in the audio settings, and close heavy
  plug-ins.
- **A plug-in is missing when a project is opened:** The program shows a list of the plug-ins and files
  that could not be loaded. Scan again in the Plug-in Manager and open the project again.

---

## Appendix A – The `.artixml` file format

An `.artixml` file is a plain **UTF-8 XML document**. It can be read and edited in any text editor or
processed with XML tools. The root element is `ARTILODY_PROJECT`.

### A.1 Overall structure

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ARTILODY_PROJECT version="1.0.6">
  <SONG title="…" bpm="128.0" lpb="4" sequence="0,0,1">
    <TRACKS>
      <TRACK … />            <!-- one per track, in order -->
    </TRACKS>
    <PATTERNS>
      <PATTERN …>            <!-- one per pattern; its number is its position (0, 1, 2 …) -->
        <T d="…"/>           <!-- one per track, same order as TRACKS -->
      </PATTERN>
    </PATTERNS>
  </SONG>
  <STRIPS>
    <STRIP …> … </STRIP>     <!-- one channel strip per track, same order -->
  </STRIPS>
  <STRIP master="1" …> … </STRIP>   <!-- the master strip -->
</ARTILODY_PROJECT>
```

Numbers are written in **decimal** in the file, including the ones shown in hex in the program. Colours are
8-digit ARGB hex, for example `ff2f7bf0` (alpha `ff`, red `2f`, green `7b`, blue `f0`). Booleans are
written as `1`/`0`.

### A.2 `ARTILODY_PROJECT`

| Attribute | Meaning |
|---|---|
| `version` | The program version that saved the file. |

### A.3 `SONG`

| Attribute | Type | Meaning |
|---|---|---|
| `title` | text | Song title |
| `bpm` | decimal | Initial tempo (20–999) |
| `lpb` | integer | Lines per beat (1–32) |
| `sequence` | comma-separated list | The sequence: pattern numbers (0-based) in playback order, for example `0,0,1,2` |

### A.4 `TRACK` (in `TRACKS`)

| Attribute | Type | Meaning |
|---|---|---|
| `name` | text | Track name |
| `colour` | ARGB hex | Track colour |
| `columns` | 1–8 | Number of note columns |
| `uid` | integer | Internal unique track id |

Track number *n* (1-based) is the *n*-th `TRACK` element. The same number is used in the Ins column.

### A.5 `PATTERN` (in `PATTERNS`)

| Attribute | Type | Meaning |
|---|---|---|
| `name` | text | Pattern name |
| `colour` | ARGB hex | Colour |
| `rows` | 1–512 | Number of lines |
| `notes` | text | The pattern's notes |

Each `PATTERN` has one `T` element per track. Its `d` attribute holds the track's **non-empty cells**
separated by spaces. Each cell is seven comma-separated integers:

```
row,column,note,ins,vol,fxCommand,fxValue
```

| Field | Range | Meaning |
|---|---|---|
| `row` | 0 – rows−1 | Line number |
| `column` | 0–7 | Note column in the track |
| `note` | 0–119, −1, −2 | MIDI note number (60 = `C-5`, 48 = `C-4`). **−1** = empty, **−2** = note off (`OFF`) |
| `ins` | 1–254, 255 | Number of the track whose instrument plays the note. **255** = empty (the track's own instrument) |
| `vol` | 0–127, 255 | Velocity/volume. **255** = empty |
| `fxCommand` | 0 or ASCII code | Character code of the effect letter: `T`=84, `L`=76, `D`=68, `C`=67, `R`=82, `P`=80, `M`=77, `B`=66. **0** = no effect |
| `fxValue` | 0–255 | The effect's value (shown as hex `xx` in the program) |

Cells not in the list are empty. `d=""` means the track is empty in this pattern.

**Example:** `0,0,60,3,72,0,0 10,1,64,3,56,68,128 14,0,-2,255,255,0,0` means:

- Line 0, column 0: `C-5`, instrument 03, vol 48 (hex), no effect.
- Line 10, column 1: `E-5`, instrument 03, vol 38, effect `D80` (68 = `D`, 128 = hex `80`).
- Line 14, column 0: `OFF`.

### A.6 `STRIP` (channel strip)

`STRIPS` holds one `STRIP` per track, in the same order as `TRACKS`. The master strip sits directly under
`ARTILODY_PROJECT` with `master="1"`.

| Attribute | Type | Meaning |
|---|---|---|
| `volume` | dB | Fader, −60 to +6 |
| `pan` | −1 – 1 | Panning (−1 = left, 0 = centre, 1 = right) |
| `mute` / `solo` | 0/1 | Mute and solo |
| `fxCollapsed` | 0/1 | Whether the plug-in list is collapsed |
| `master` | 1 | Master strip only |

A `STRIP` contains three child elements: `INSTRUMENT`, `FXCHAIN` and `BUILTINS`.

#### `INSTRUMENT`

The `kind` attribute sets the type:

| `kind` | Other attributes |
|---|---|
| `soundfont` | `genre` (Pop/Rock/Electro/Jazz or empty), `name` (preset name), `bank` (128 = drum kits), `program` (0–127), `drums` (0/1), `file` (only when a `.sf2` other than the default is used: full path) |
| `sample` | `file` (full path to the audio file), `root` (root note, default 60) |
| `plugin` | Contains one `PLUGIN` element (see below) |
| `none` | No instrument (always the case on the master strip) |

#### `FXCHAIN`

Contains one `PLUGIN` element per plug-in effect, in signal order. Each element also has a `bypass`
attribute (0/1).

#### `PLUGIN`

```xml
<PLUGIN state="…base64…" bypass="0">
  <PLUGIN name="…" format="VST3" manufacturer="…" file="…" uniqueId="…" isInstrument="0" … />
</PLUGIN>
```

- The `state` attribute is the plug-in's own state (its "chunk"), base64-encoded. Its contents are
  defined by the plug-in.
- The inner `PLUGIN` element is JUCE's *PluginDescription*. It is used to find and reload the plug-in:
  name, format (`VST3`, `AudioUnit`, `CLAP`), vendor, file/identifier, unique id and so on.

#### `BUILTINS`

Contains three `FX` elements for the built-in effects. Each has `name` and `enabled` (0/1).

`Compressor` and `Reverb` have one attribute per parameter:

| `name` | Parameters (attribute name = value) |
|---|---|
| `Compressor` | `threshold` (dB), `ratio`, `attack` (ms), `release` (ms), `knee` (dB), `makeup` (dB) |
| `Reverb` | `size`, `damping`, `width`, `mix` (0–1) |

`EQ` instead has one `BAND` child element per band (at most 8):

```xml
<FX name="EQ" enabled="1">
  <BAND type="lowcut" freq="40.0" gain="0.0" q="0.7071"/>
  <BAND type="bell" freq="2500.0" gain="5.0" q="0.8"/>
</FX>
```

| Attribute | Meaning |
|---|---|
| `type` | `bell`, `lowshelf`, `highshelf`, `lowcut`, `highcut` or `notch` |
| `freq` | Frequency in Hz (20–20000) |
| `gain` | Gain in dB (−24–24). Only used by `bell` and the shelf types |
| `q` | Q / bandwidth (0.1–18) |

Files from version 1.0.0–1.0.5 stored the EQ as fixed attributes (`lowcut`, `lowfreq`, `lowgain`, `midfreq`,
`midgain`, `midq`, `hifreq`, `higain`). They still load and are converted to bands automatically.

Ranges and default values are listed in section 12. If a parameter is missing, its default value is used.

### A.7 Robustness and hand editing

The program tolerates many deviations in the file:

- Unknown elements and attributes are ignored.
- Missing values get their defaults.
- Sequence numbers, columns and row counts are clamped to valid ranges.
- Cells outside the pattern are skipped.
- Plug-ins or files that cannot be found are reported when the project is opened. The rest of the project
  still loads.

If you edit a file by hand, the number of `TRACK`, `T` (per pattern) and `STRIP` (in `STRIPS`) elements must
match.

### A.8 Complete minimal example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ARTILODY_PROJECT version="1.0.6">
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

This example plays the notes of a C major chord (C-5, E-5, G-5) as quarter notes on a piano and releases the
note on line 12.
