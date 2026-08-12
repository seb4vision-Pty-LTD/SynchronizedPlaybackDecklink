# SyncedPlaybackDecklink

This project plays back 8 video files on a Decklink quad card.
On the interface, the user must select the root directory containing sessions.

## Folder Structure

```
Root Dir/
├── Session 1/
│   └── Playlist 1/
│       ├── Video1.mp4
│       ├── Video2.mp4
│       ├── Video3.mp4
│       ├── Video4.mp4
│       ├── Video5.mp4
│       └── Video6.mp4
└── Session 2/
    └── Playlist 1/
        ├── Video1.mp4
        ├── Video2.mp4
        ├── Video3.mp4
        ├── Video4.mp4
        ├── Video5.mp4
        └── Video6.mp4
```

## Controls

| Key     | Action      |
|---------|-------------|
| `A`     | Previous    |
| `Z`     | Next        |
| `Space` | Pause/Play  |

## Channel Mapping

| Decklink Device | Channel   |
|-----------------|-----------|
| Device 1        | Channel 1 |
| Device 2        | Channel 2 |
| Device 3        | Channel 3 |
| Device 4        | Channel 4 |
| Device 5        | Channel 5 |
| Device 6        | Channel 6 |
| Device 7        | Channel 7 |
| Device 8        | Channel 8 |


# Support input modes
> Video files must be mp4 (25 fps (interlaced/mixed))

> **Decklink output**: 1080p25


## Notes

> **Driver Required:** The application requires Decklink drivers to be installed — they are not bundled with the application.

> **Device Mode:** Decklink devices must be set to **Half-duplex** mode (`SDI1 → Device 1`), not full-duplex mode (`SDI1 → Device 1 & Device 2`).

## Build Instructions


> cmake -S . -B build

> cmake --build build --target Release

## Build Requirements

1. Qt6
2. Gstreamer 1.28
3. Microsoft visual c++ x86_amd64 compiler
4. Gstreamer plugins (good, bad, system) {All plugins bundled with gstreamer installer for windows}
