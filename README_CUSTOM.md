# XTEINK X4 Custom Firmware (Lee's Edition)

This is a highly customized firmware loop for the XTEINK X4 e-ink reader. It focuses on minimalism, reading immersion, and adding personalized utility modules, while aggressively debloating unused features to optimize the ESP32-C3's limited memory.

---

## ✨ Key Additions & Modules

- **Global Reading Statistics (`ReadingStatsActivity`)**: A centralized tracking system that records reading time across all formats (`.epub`, `.xtc`, `.txt`). Tracks time using the hardware `millis()` timer. View total accumulated time and per-book engagement in the System settings.
- **3D Tic-Tac-Toe Game (`QubicActivity`)**: A perfectly optimized, memory-safe 3D Tic-Tac-Toe game playable directly on the e-ink screen.
- **Flashcard System**: A custom module designed for English vocabulary learning directly on the device.
- **Floating Menu System**: A unified, modern floating menu architecture used in Qubic, XTC, and EPUB readers. Features a 10px rounded corner aesthetic and optimized refresh logic.
- **Bayer Dithering Algorithm**: Uses an 8x8 Bayer Matrix for high-quality thumbnail rendering.
- **High-Contrast Text Anti-Aliasing**: Smooths UI text using a "Solid Core + Dithered Edges" strategy for maximum readability.

---

## 🎮 Interface & Button Layout

The button logic has been overhauled for better ergonomics, using "clusters" to make the small physical buttons easier to use.

### 1. Global Button Mapping

The 4 front physical buttons are grouped into two logical clusters. The **Remap Front Buttons** option has been removed from Settings to maintain this intuitive layout.

| Physical Buttons                                | Logical Role               |
| ----------------------------------------------- | -------------------------- |
| `BTN_BACK` + `BTN_CONFIRM` (Front-Left Cluster) | **Back / Previous**        |
| `BTN_LEFT` + `BTN_RIGHT` (Front-Right Cluster)  | **Confirm / Next / Menu**  |
| `BTN_UP` / `BTN_DOWN` (Side Buttons)            | **Up / Down / Navigation** |

### 2. Reader Layout (Epub / XTC / Txt)

| Button                  | Short Press   | Long Press (≥ 600ms) |
| ----------------------- | ------------- | -------------------- |
| **Side Up**             | Next page     | +10 pages            |
| **Side Down**           | Previous page | −10 pages            |
| **Front-Left Cluster**  | Previous page | Exit to Home         |
| **Front-Right Cluster** | Next page     | Open Floating Menu   |

### 3. Home Screen & Navigation

- **Front-Right Cluster**: Select / Confirm.
- **Side Buttons**: Navigate lists.
- **Button Hints**: Removed from the bottom of all screens to maximize vertical screen real estate.

---

## 🛠️ Technical Details & Fixes

### XTC Reader Status Bar

Added a custom `renderStatusBar()` to the XTC reader. This overlays a progress bar and page count on top of pre-rendered XTC bitmaps, matching the functionality of the official EPUB reader.

### Aggressive Debloat

- Removed 11 language packs (English/Baseline only).
- Deleted `RoundedRaff` theme engine to free up RAM.
- Removed network-heavy features (KOReader Sync, OPDS, OTA) for better contiguous memory stability.
- Pruned built-in fonts, standardizing on **Bookerly** and **Noto Sans**.

---

## � SD Card File Structure

To ensure all custom modules (XTC Reader, Flashcards) work correctly, the SD card should follow this structure:

```text
SD Card Root/
├── books/           # .epub and .xtc files
└── flashcard/       # Flashcard BMP images (e.g., 001_a.bmp, 001_b.bmp)
```

Sample data is provided in the `sdcard/` subdirectory of this repository for reference.

---

## �📁 Maintenance & Tools

### Flashcard Generation

Flashcards are generated via a Python script at `Flashcard_Sleep/gen_flashcards.py`. It converts Markdown word lists into landscape 8-bit grayscale BMPs.

### Syncing with Upstream

To sync future updates from the original repository:

1. `git remote add upstream https://github.com/daveallie/crosspoint-reader.git`
2. `git fetch upstream`
3. `git merge upstream/master`
