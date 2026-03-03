# XTEINK X4 Custom Firmware (Lee's Edition)

This is a highly customized firmware loop for the XTEINK X4 e-ink reader. It focuses on minimalism, reading immersion, and adding personalized utility modules, while aggressively debloating unused features to optimize the ESP32-C3's extremely limited memory (~380KB RAM).

## ✨ Key Additions & Modules

- **Global Reading Statistics (`ReadingStatsActivity`)**: A centralized tracking system that records reading time across all formats (`.epub`, `.xtc`, `.txt`). Because the ESP32 lacks an RTC, it tracks time using the hardware `millis()` timer. View total accumulated time and per-book engagement in the System settings.
- **3D Tic-Tac-Toe Game (`QubicActivity`)**: A perfectly optimized, memory-safe 3D Tic-Tac-Toe game playable directly on the e-ink screen. (Includes a sophisticated Use-After-Free memory fix during exit transitions).
- **Flashcard System**: A custom module designed for English vocabulary learning directly on the device.
- **Floating Menu System**: A unified, modern floating menu architecture used in Qubic, XTC, and EPUB readers. Features a 10px rounded corner aesthetic, rounded selection highlights, and adaptive positioning. It provides a "Premium E-Ink" experience without sacrificing 1-bit refresh speeds.
- **Bayer Dithering Algorithm**: Replaced the chaotic random noise dithering with an industry-standard 8x8 Bayer Matrix Ordered Dithering algorithm for image thumbnails.
- **High-Contrast Text Anti-Aliasing**: Implemented in `GfxRenderer` to provide smoother UI text. It uses a "Solid Core + Dithered Edges" strategy (effectively 2-bit grayscale simulated via 1-bit dithering), making small fonts readable and smooth without appearing fuzzy.

## 🎨 UI/UX Refinements

- **Refined Status Bar & Auxiliary Info**: Page numbers and secondary UI elements now use a 50% chessboard dither (`Color::DarkGray`) to recede visually, reducing distraction. In Dark Mode, this automatically inverts to a white-dithered gray.
- **Minimalist Layout**: Removed battery icon/progress bars from the status bar for a clean, centered "current/total page" configuration.
- **Recent Books Layout**: Enhanced the 9-grid Recent Books menu with minimal selector borders and optimized spacing.
- **CJK Font Fallbacks**: Optimized metadata extraction to prevent squares/tofu characters without excessive memory overhead.

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
