# XTEINK X4 Custom Firmware (Lee's Edition)

This is a highly customized firmware loop for the XTEINK X4 e-ink reader. It focuses on minimalism, reading immersion, and adding personalized utility modules, while aggressively debloating unused features to optimize the ESP32-C3's extremely limited memory (~380KB RAM).

## ✨ Key Additions & Modules

- **Global Reading Statistics (`ReadingStatsActivity`)**: A centralized tracking system that records reading time across all formats (`.epub`, `.xtc`, `.txt`). Because the ESP32 lacks an RTC, it tracks time using the hardware `millis()` timer. View total accumulated time and per-book engagement in the System settings.
- **3D Tic-Tac-Toe Game (`QubicActivity`)**: A perfectly optimized, memory-safe 3D Tic-Tac-Toe game playable directly on the e-ink screen. (Includes a sophisticated Use-After-Free memory fix during exit transitions).
- **Flashcard System**: A custom module designed for English vocabulary learning directly on the device.
- **Bayer Dithering Algorithm**: Replaced the chaotic random noise dithering with an industry-standard 8x8 Bayer Matrix Ordered Dithering algorithm. This results in beautiful, newspaper-quality halftone rendering for all 1-bit book cover thumbnails.
- **Custom Boot Logo**: Integrated a personalized seal as the 120x120 E-Ink boot screen logo, properly rotated to exactly align with the specific screen orientation.

## 🎨 UI/UX Refinements

- **Minimalist Status Bar**: Removed all battery icons, progress bars, and clutter. Now displays only a clean, centered "current/total page" text configuration to maximize reading immersion and reduce e-ink refresh artifacting.
- **Recent Books Layout**: Enhanced the 9-grid Recent Books menu by restricting dynamic cover scaling, adding 1px minimalist selector borders, and widening horizontal/vertical cover spacing for a cleaner aesthetic.
- **CJK Font Fallbacks**: Ensured the UI safely extracts actual filenames instead of missing or un-renderable metadata (which previously displayed as squares), bypassing the memory-overflow issues seen in other CJK forks.

## 🗑️ Aggressive Debloat (Memory Optimization)

To ensure absolute stability on the ESP32-C3, the following components were removed:

- **Language Packs Removed**: Stripped 11 multi-language packs. English is retained as the baseline, saving massive amounts of string-table Flash space.
- **Themes Removed**: Completely deleted the heavy `RoundedRaff` theme engine and its independent rendering classes.
- **Network Bloat Removed**: Extracted KOReader Sync, OPDS online bookstore downloading, and OTA update wrappers to free up contiguous memory for seamless reading performance.
- **Built-in Fonts Pruned**: Unused fonts (like OpenDyslexic, Noto Sans TC) were removed from the embedded assets to save space, standardizing specifically around Noto Sans and Bookerly.

## 🛠️ Upstream Merge Strategy

_(For maintaining compatibility with the original `crosspoint-reader` source)_

Custom modules (Qubic, Flashcards, ReadingStats) have been built as **isolated `.cpp` / `.h` components**.

To sync future updates from the original author:

1. `git remote add upstream https://github.com/daveallie/crosspoint-reader.git`
2. `git fetch upstream`
3. `git merge upstream/master`

During standard git merges, the custom isolated module files will not cause conflicts. Minor UI tweaks in core hubs (`HomeActivity.cpp`, `EpubReaderActivity.cpp`, `CrossPointSettings.cpp`) may require manual conflict resolution, but Git's line-by-line analyzer handles the vast majority flawlessly. The modular architecture ensures the custom features remain intact.
