#include "FlashcardActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <esp_random.h>
#include <algorithm>
#include "components/UITheme.h"
#include "fontIds.h"

FlashcardActivity::FlashcardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     std::function<void()> onGoBack)
    : Activity("Flashcard", renderer, mappedInput), onGoBack(std::move(onGoBack)) {}

#include <ArduinoJson.h>

void FlashcardActivity::onEnter() {
  Activity::onEnter();
  loadDecks();
  if (decks.empty()) {
    // Fallback to root /flashcard if no subdirs
    selectedDeckName = "";
    inDeckSelection = false;
    loadFileList("");
    showRandomCard();
  } else {
    inDeckSelection = true;
    renderDeckMenu(false); // Fast refresh on entry for speed
  }
}

void FlashcardActivity::onExit() {
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void FlashcardActivity::loadDecks() {
  decks.clear();
  auto root = Storage.open("/flashcard");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  root.rewindDirectory();
  char name[128];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    if (file.isDirectory()) {
      file.getName(name, sizeof(name));
      if (name[0] != '.' && strcmp(name, "System Volume Information") != 0) {
        decks.push_back(name);
      }
    }
    file.close();
  }
  root.close();
  std::sort(decks.begin(), decks.end());
}

void FlashcardActivity::renderDeckMenu(bool fullRefresh) {
  renderer.clearScreen();
  int screenWidth = renderer.getScreenWidth();
  int y = 40;
  
  renderer.drawCenteredText(UI_12_FONT_ID, y, "Select Flashcard Deck");
  y += 60;

  for (int i = 0; i < (int)decks.size(); ++i) {
    int itemHeight = 40;
    if (i == deckSelectedIndex) {
      renderer.fillRoundedRect(40, y - 5, screenWidth - 80, itemHeight, 8, Color::Black);
      renderer.drawText(UI_10_FONT_ID, 60, y + 10, decks[i].c_str(), false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 60, y + 10, decks[i].c_str(), true);
    }
    y += itemHeight + 10;
  }
  renderer.displayBuffer(fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
}

void FlashcardActivity::renderSubMenu() {
  // Use the stored background if it exists
  // renderSubMenu is called after storeBwBuffer() in loop()
  renderer.restoreBwBuffer(); 
  
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  int pw = renderer.getScreenWidth();
  int ph = renderer.getScreenHeight();
  
  int menuWidth = 300;
  int menuHeight = 280;
  int x = (pw - menuWidth) / 2;
  int y = (ph - menuHeight) / 2;
  
  // Draw menu box
  renderer.fillRoundedRect(x, y, menuWidth, menuHeight, 10, Color::White);
  renderer.drawRoundedRect(x, y, menuWidth, menuHeight, 2, 10, true);
  
  const char* options[] = {"Resume", "Go to Front", "Select Deck", "Reset Progress", "Exit"};
  int optionCount = 5;
  
  for (int i = 0; i < optionCount; i++) {
    int oy = y + 40 + (i * 45); // Slightly tighter spacing for 5 items
    if (i == subMenuSelectedIndex) {
      renderer.fillRoundedRect(x + 20, oy - 5, menuWidth - 40, 40, 8, Color::Black);
      renderer.drawCenteredText(UI_10_FONT_ID, oy + 5, options[i], false);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, oy + 5, options[i], true);
    }
  }
  
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
}

void FlashcardActivity::loadFileList(const std::string& deckName) {
  cards.clear();
  selectedDeckName = deckName;
  std::string basePath = "/flashcard";
  if (!deckName.empty()) {
    basePath += "/" + deckName;
  }
  
  std::vector<String> files = Storage.listFiles(basePath.c_str(), 500);
  
  std::vector<std::string> sideAFiles;
  for (const auto& f : files) {
    if (f.startsWith(".")) continue;
    std::string filename = f.c_str();
    if (filename.size() > 4 && (filename.substr(filename.size() - 4) == ".bmp" || filename.substr(filename.size() - 4) == ".BMP")) {
      if (filename.find("_a.") != std::string::npos || filename.find("_A.") != std::string::npos) {
        sideAFiles.push_back(filename);
      }
    }
  }
  std::sort(sideAFiles.begin(), sideAFiles.end());

  for (const auto& sideA : sideAFiles) {
    FlashCardPair pair;
    pair.sideA = basePath + "/" + sideA;
    
    std::string sideB = sideA;
    size_t pos = sideB.find("_a.");
    if (pos == std::string::npos) pos = sideB.find("_A.");
    if (pos != std::string::npos) {
      sideB.replace(pos, 2, sideA[pos] == '_' ? "_b" : "_B");
      bool exists = false;
      for (const auto& f : files) {
        if (std::string(f.c_str()) == sideB) {
          exists = true;
          break;
        }
      }
      if (exists) pair.sideB = basePath + "/" + sideB;
    }
    cards.push_back(pair);
  }

  if (cards.empty()) {
    for (const auto& f : files) {
      if (f.startsWith(".") || (!f.endsWith(".bmp") && !f.endsWith(".BMP"))) continue;
      cards.push_back({basePath + "/" + std::string(f.c_str()), "", 0, false});
    }
  }
  
  loadProgress();
}

void FlashcardActivity::loadProgress() {
  std::string path = "/flashcard/" + selectedDeckName + "/stats.json";
  if (!Storage.exists(path.c_str())) return;

  String content = Storage.readFile(path.c_str());
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error) return;

  JsonVariant cardsJson = doc["cards"];
  if (!cardsJson.is<JsonObject>()) return;

  JsonObject root = cardsJson.as<JsonObject>();
  for (auto& pair : cards) {
    JsonVariant stats = root[pair.sideA.c_str()];
    if (!stats.isNull()) {
      pair.viewCount = stats["count"] | 0;
      pair.learned = stats["learned"] | false;
    }
  }
}

void FlashcardActivity::saveProgress() {
  std::string path = "/flashcard/" + selectedDeckName + "/stats.json";
  JsonDocument doc;
  JsonObject root = doc["cards"].to<JsonObject>();

  for (const auto& pair : cards) {
    if (pair.viewCount > 0 || pair.learned) {
        JsonObject stats = root[pair.sideA.c_str()].to<JsonObject>();
        stats["count"] = pair.viewCount;
        stats["learned"] = pair.learned;
    }
  }

  String output;
  serializeJson(doc, output);
  Storage.writeFile(path.c_str(), output);
}

void FlashcardActivity::showRandomCard() {
  if (cards.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, "No cards found");
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  std::vector<int> candidates;
  std::vector<double> weights;
  double totalWeight = 0;

  for (int i = 0; i < (int)cards.size(); i++) {
    if (!cards[i].learned) {
      candidates.push_back(i);
      double w = 1.0 / (1.0 + cards[i].viewCount);
      weights.push_back(w);
      totalWeight += w;
    }
  }

  if (candidates.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 180, "🎉 Deck Completed!");
    renderer.drawCenteredText(UI_10_FONT_ID, 240, "Long press Left to reset or exit.");
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  double r = (double)esp_random() / UINT32_MAX * totalWeight;
  double cumulative = 0;
  int selected = candidates.back();
  for (size_t i = 0; i < candidates.size(); i++) {
    cumulative += weights[i];
    if (r <= cumulative) {
      selected = candidates[i];
      break;
    }
  }

  currentIndex = selected;
  isShowingBack = false;
  cards[currentIndex].viewCount++;
  saveProgress();
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::showNextCard() {
  if (cards.empty()) return;
  int start = currentIndex;
  do {
    currentIndex = (currentIndex + 1) % cards.size();
  } while (cards[currentIndex].learned && currentIndex != start);
  
  isShowingBack = false;
  cards[currentIndex].viewCount++;
  saveProgress();
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::showPrevCard() {
  if (cards.empty()) return;
  int start = currentIndex;
  do {
    currentIndex = (currentIndex - 1 + (int)cards.size()) % cards.size();
  } while (cards[currentIndex].learned && currentIndex != start);

  isShowingBack = false;
  cards[currentIndex].viewCount++;
  saveProgress();
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::showFirstCard() {
  if (cards.empty()) return;
  currentIndex = 0;
  isShowingBack = false;
  cards[currentIndex].viewCount++;
  saveProgress();
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::jumpCards(int delta) {
  if (cards.empty()) return;
  int target = currentIndex + delta;
  if (target < 0) target = 0;
  if (target >= (int)cards.size()) target = cards.size() - 1;
  
  currentIndex = target;
  isShowingBack = false;
  cards[currentIndex].viewCount++;
  saveProgress();
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::toggleFlip() {
  if (cards.empty() || currentIndex < 0) return;
  if (cards[currentIndex].sideB.empty()) return;
  isShowingBack = !isShowingBack;
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::markAsLearned() {
  if (currentIndex < 0 || currentIndex >= (int)cards.size()) return;
  cards[currentIndex].learned = true;
  saveProgress();
  
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  
  int tw = 500;
  int th = 80;
  int tx = (renderer.getScreenWidth() - tw) / 2;
  int ty = (renderer.getScreenHeight() - th) / 2;
  
  renderer.fillRoundedRect(tx, ty, tw, th, 15, Color::White);
  renderer.drawRoundedRect(tx, ty, tw, th, 3, 15, true);
  renderer.drawCenteredText(UI_12_FONT_ID, ty + 25, "Great! You have learned a new word.", true);
  
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  delay(1500); 
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  
  showNextCard();
}

void FlashcardActivity::renderCard(int index, bool isBack, bool fullRefresh) {
  if (index < 0 || index >= (int)cards.size()) return;
  
  const std::string& path = isBack ? cards[index].sideB : cards[index].sideA;
  if (path.empty()) return;

  FsFile file;
  if (Storage.openFileForRead("FLASH", path, file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
      renderer.clearScreen();
      int pw = renderer.getScreenWidth();
      int ph = renderer.getScreenHeight();
      int x = (pw - bitmap.getWidth()) / 2;
      int y = (ph - bitmap.getHeight()) / 2;
      renderer.drawBitmap(bitmap, std::max(0, x), std::max(0, y), pw, ph);
      
      // Grayscale Minimal UI Overlays (Small Font)
      // black=false in drawText results in LightGray dither
      
      // Top-left: Deck Name 
      renderer.drawText(SMALL_FONT_ID, 30, 30, selectedDeckName.c_str(), true); 
      
      // Bottom-left: Consolidated Status
      char statusLine[64];
      int learnedCount = 0;
      for (const auto& c : cards) if (c.learned) learnedCount++;
      snprintf(statusLine, sizeof(statusLine), "%d / %u | Learned: %d", 
               index + 1, (unsigned int)cards.size(), learnedCount);
      renderer.drawText(SMALL_FONT_ID, 30, ph - 30, statusLine, true);
      
      // Right edge: Rotated Button Hints (Plain text, rotated)
      int hintX = pw - 30; // Safely away from the 800 edge
      int hintY1 = 400;    // Top cluster (labels RANDOM / EXIT)
      int hintY2 = 200;    // Bottom cluster (labels FLIP / LEARNED)
      
      renderer.drawTextRotated90CW(SMALL_FONT_ID, hintX, hintY1, "RANDOM / EXIT", true);
      renderer.drawTextRotated90CW(SMALL_FONT_ID, hintX, hintY2, "FLIP / LEARNED", true);

      renderer.displayBuffer(fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
      renderer.setOrientation(GfxRenderer::Orientation::Portrait);
    }
    file.close();
  }
}

void FlashcardActivity::loop() {
  Activity::loop();
  const unsigned long longPressMs = 500;

  if (inDeckSelection) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      deckSelectedIndex = (deckSelectedIndex - 1 + (int)decks.size()) % decks.size();
      renderDeckMenu(false);
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      deckSelectedIndex = (deckSelectedIndex + 1) % decks.size();
      renderDeckMenu(false);
    } else if (mappedInput.wasReleasedAnyOf(HalGPIO::BTN_LEFT, HalGPIO::BTN_RIGHT)) {
      inDeckSelection = false;
      loadFileList(decks[deckSelectedIndex]);
      showRandomCard();
    } else if (mappedInput.wasReleasedAnyOf(HalGPIO::BTN_BACK, HalGPIO::BTN_CONFIRM)) {
      if (mappedInput.getHeldTime() >= longPressMs) { if (onGoBack) onGoBack(); }
    }
    return;
  }

  if (inSubMenu) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      subMenuSelectedIndex = (subMenuSelectedIndex - 1 + 5) % 5;
      renderSubMenu();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      subMenuSelectedIndex = (subMenuSelectedIndex + 1) % 5;
      renderSubMenu();
    } else if (mappedInput.wasReleasedAnyOf(HalGPIO::BTN_LEFT, HalGPIO::BTN_RIGHT) || 
               mappedInput.wasReleasedAnyOf(HalGPIO::BTN_BACK, HalGPIO::BTN_CONFIRM)) {
      inSubMenu = false;
      if (subMenuSelectedIndex == 0) { renderCard(currentIndex, isShowingBack, false); } 
      else if (subMenuSelectedIndex == 1) { showFirstCard(); } 
      else if (subMenuSelectedIndex == 2) { inDeckSelection = true; renderDeckMenu(true); } 
      else if (subMenuSelectedIndex == 3) { 
        for (auto& c : cards) { c.viewCount = 0; c.learned = false; }
        saveProgress();
        showRandomCard();
      }
      else if (subMenuSelectedIndex == 4) { if (onGoBack) onGoBack(); } 
    }
    return;
  }

  // Vertical side buttons (Single buttons)
  if (mappedInput.wasLongPressedRaw(HalGPIO::BTN_UP, longPressMs)) jumpCards(-10);
  else if (mappedInput.wasLongPressedRaw(HalGPIO::BTN_DOWN, longPressMs)) jumpCards(10);
  else if (mappedInput.wasShortPressedRaw(HalGPIO::BTN_UP, longPressMs)) showPrevCard();
  else if (mappedInput.wasShortPressedRaw(HalGPIO::BTN_DOWN, longPressMs)) showNextCard();

  // Left Pair (BACK + CONFIRM) -> Random Card / Menu
  bool leftPairLong = mappedInput.isLongPressed(MappedInputManager::Button::Back, longPressMs); // Using logical mapping which groups them
  
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Back, longPressMs)) {
    renderer.storeBwBuffer();
    inSubMenu = true; 
    subMenuSelectedIndex = 0;
    renderSubMenu();
  } 
  else if (mappedInput.wasShortPressed(MappedInputManager::Button::Back, longPressMs)) {
    showRandomCard();
  }

  // Right Pair (LEFT + RIGHT) -> Flip / Learned
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, longPressMs)) {
    markAsLearned();
  } 
  else if (mappedInput.wasShortPressed(MappedInputManager::Button::Confirm, longPressMs)) {
    toggleFlip();
  }
}
