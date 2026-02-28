# Lee's CrossPoint Reader Customizations

All changes are contained in a single git commit on the `lee/custom-layout` branch,
on top of the official `v1.1-stable` tag from https://github.com/crosspoint-reader/crosspoint-reader.

---

## 1. Global Front Button Remap

**File:** `src/MappedInputManager.cpp` → `mapButton()`

The 4 front physical buttons are hardcoded into two logical clusters, bypassing the per-user remap setting.
The **Remap Front Buttons** option has also been removed from Settings → Controls.

| Physical Buttons                                | Logical Role                 |
| ----------------------------------------------- | ---------------------------- |
| `BTN_BACK` + `BTN_CONFIRM` (front-left cluster) | Logical **Back**             |
| `BTN_LEFT` + `BTN_RIGHT` (front-right cluster)  | Logical **Confirm / Select** |
| `BTN_UP` / `BTN_DOWN` (side buttons)            | Unchanged                    |

**Why:** The original 4 individual front buttons were small and easy to mis-press. Merging each pair into a cluster makes the interface much more forgiving.

New raw GPIO helpers added to `MappedInputManager` (used by reader activities):

- `wasReleasedRaw(btn)` / `isPressedRaw(btn)`
- `wasReleasedAnyOf(a, b)` / `isPressedAnyOf(a, b)`

---

## 2. Reader Button Layout (Epub / XTC / TXT)

**Files:**

- `src/activities/reader/EpubReaderActivity.cpp`
- `src/activities/reader/XtcReaderActivity.cpp`
- `src/activities/reader/TxtReaderActivity.cpp`

Reader activities bypass the logical remap and read raw GPIO directly.

| Button                           | Short Press   | Long Press (≥ 600ms)     |
| -------------------------------- | ------------- | ------------------------ |
| **Side Up**                      | Next page     | +10 pages                |
| **Side Down**                    | Previous page | −10 pages                |
| **Front-left** (BACK or CONFIRM) | Previous page | Go home                  |
| **Front-right** (LEFT or RIGHT)  | Next page     | Open menu / chapter list |

> **Important:** Long press triggers are detected **on button release** (not while held).
> This prevents the stale release event from being picked up by the newly opened menu screen.

---

## 3. Home Screen Button Layout

**File:** `src/activities/home/HomeActivity.cpp`

| Button                                                      | Action                                        |
| ----------------------------------------------------------- | --------------------------------------------- |
| **Front-right cluster** (LEFT or RIGHT, or logical Confirm) | Select item                                   |
| **Side Up / Down**                                          | Navigate list (unchanged via ButtonNavigator) |
| **Front-left cluster**                                      | No action (already at top level)              |

Button hint bar at the bottom has been **removed**.

---

## 4. Flashcard Activity Button Layout

**File:** `src/activities/util/FlashcardActivity.cpp`

| Button                                   | Short Press                | Long Press (≥ 800ms) |
| ---------------------------------------- | -------------------------- | -------------------- |
| **Front-left cluster** (BACK or CONFIRM) | Random card                | Exit to home         |
| **Front-right cluster** (LEFT or RIGHT)  | Flip card                  | Exit to home         |
| **Side Up**                              | Previous card (sequential) | —                    |
| **Side Down**                            | Next card (sequential)     | —                    |

Button hint bar at the bottom has been **removed**.

---

## 5. Flashcard Activity — New Feature

**Files:**

- `src/activities/util/FlashcardActivity.cpp` / `.h`

A new Flashcard activity was added. Cards are stored as BMP image pairs (sideA / sideB)
on the SD card. The activity supports:

- **Flip** between front and back of a card
- **Sequential navigation** (side buttons)
- **Random shuffle** to a random card

_(See earlier conversation history for details on card file format and generation script.)_

---

## 6. Button Hints Removed from UI Screens

Bottom button hint bars removed from:

| Screen              | File                                                     |
| ------------------- | -------------------------------------------------------- |
| Browse (My Library) | `src/activities/home/MyLibraryActivity.cpp`              |
| Recent Books        | `src/activities/home/RecentBooksActivity.cpp`            |
| Settings            | `src/activities/settings/SettingsActivity.cpp`           |
| File Transfer       | `src/activities/network/CrossPointWebServerActivity.cpp` |

The list `contentHeight` in Browse and Recent was also expanded to fill the space previously occupied by the hint bar.

---

## Upgrade Instructions

When a new official release is available:

```bash
# Fetch new upstream commits
git fetch origin

# Rebase our custom commits onto the new upstream
git checkout lee/custom-layout
git rebase origin/master
```

Resolve any conflicts (most likely in `MappedInputManager.cpp` and reader activity files),
then rebuild and flash with `pio run -t upload`.

---

## 7. XTC Reader — Status Bar Fix

**File:** `src/activities/reader/XtcReaderActivity.cpp` / `.h`

The original code assumed the status bar was pre-baked into each XTC page bitmap.
In practice, the converter does not include a status bar, so nothing was displayed.

**Fix:** Added `renderStatusBar()` method that overlays the status bar on top of the rendered
page bitmap after each page is drawn. Applies to both 1-bit and 2-bit (grayscale) render paths.

Behavior is identical to the EPUB reader, respecting `Settings → Status Bar` mode:

| Mode              | Elements Shown              |
| ----------------- | --------------------------- |
| Full              | Page X/Y, book %, battery   |
| Book Progress Bar | Page X/Y, thin bar, battery |
| No Progress       | Battery only                |
| None              | Nothing                     |
