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

void FlashcardActivity::onEnter() {
  Activity::onEnter();
  loadFileList();
  if (!cards.empty()) {
    uint32_t r = esp_random() % cards.size();
    currentIndex = static_cast<int>(r);
    isShowingBack = false;
    renderCard(currentIndex, isShowingBack, true); // Initial card Full Refresh
  } else {
    showRandomCard();
  }
}

void FlashcardActivity::onExit() {
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void FlashcardActivity::loadFileList() {
  cards.clear();
  std::vector<String> files = Storage.listFiles("/flashcard", 500);
  
  // First, find all side-A files
  std::vector<std::string> sideAFiles;
  for (const auto& f : files) {
    if (f.startsWith(".")) continue;
    std::string filename = f.c_str();
    if (filename.substr(filename.find_last_of(".") + 1) == "bmp" || 
        filename.substr(filename.find_last_of(".") + 1) == "BMP") {
      if (filename.find("_a.") != std::string::npos || filename.find("_A.") != std::string::npos) {
        sideAFiles.push_back(filename);
      }
    }
  }
  std::sort(sideAFiles.begin(), sideAFiles.end());

  for (const auto& sideA : sideAFiles) {
    FlashCardPair pair;
    pair.sideA = "/flashcard/" + sideA;
    
    // Construct expected side B name
    std::string sideB = sideA;
    size_t pos = sideB.find("_a.");
    if (pos == std::string::npos) pos = sideB.find("_A.");
    if (pos != std::string::npos) {
      sideB.replace(pos, 2, sideA[pos] == '_' ? "_b" : "_B"); // match case
      // Check if side B exists
      bool exists = false;
      for (const auto& f : files) {
        if (std::string(f.c_str()) == sideB) {
          exists = true;
          break;
        }
      }
      if (exists) {
        pair.sideB = "/flashcard/" + sideB;
      }
    }
    cards.push_back(pair);
  }

  // Fallback: If no _a pairs found, maybe treat every BMP as sideA?
  if (cards.empty()) {
    std::vector<std::string> allFiles;
    for (const auto& f : files) {
      if (f.startsWith(".")) continue;
      if (f.endsWith(".bmp") || f.endsWith(".BMP")) {
        allFiles.push_back("/flashcard/" + std::string(f.c_str()));
      }
    }
    std::sort(allFiles.begin(), allFiles.end());
    for (const auto& f : allFiles) {
      cards.push_back({f, ""});
    }
  }
}

void FlashcardActivity::showRandomCard() {
  if (cards.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, "No cards in /flashcard");
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
    return;
  }

  uint32_t r = esp_random() % cards.size();
  currentIndex = static_cast<int>(r);
  isShowingBack = false;
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::showNextCard() {
  if (cards.empty()) return;
  currentIndex = (currentIndex + 1) % cards.size();
  isShowingBack = false;
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::showPrevCard() {
  if (cards.empty()) return;
  currentIndex = (currentIndex - 1 + (int)cards.size()) % cards.size();
  isShowingBack = false;
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::toggleFlip() {
  if (cards.empty() || currentIndex < 0) return;
  if (cards[currentIndex].sideB.empty()) return; // Nothing to flip to
  isShowingBack = !isShowingBack;
  renderCard(currentIndex, isShowingBack, false);
}

void FlashcardActivity::renderCard(int index, bool isBack, bool fullRefresh) {
  if (index < 0 || index >= (int)cards.size()) return;
  
  const std::string& path = isBack ? cards[index].sideB : cards[index].sideA;
  if (path.empty()) return;

  FsFile file;
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  if (Storage.openFileForRead("FLASH", path, file)) {
    Bitmap bitmap(file, true);
    BmpReaderError err = bitmap.parseHeaders();
    if (err == BmpReaderError::Ok) {
      // Center the image
      int x = (pageWidth - bitmap.getWidth()) / 2;
      int y = (pageHeight - bitmap.getHeight()) / 2;

      renderer.clearScreen();
      renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0);

      // No button hints
      renderer.displayBuffer(fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
    } else {
      renderer.clearScreen();
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Invalid BMP");
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    }
    file.close();
  } else {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Open Failed");
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}

void FlashcardActivity::loop() {
  Activity::loop();

  const unsigned long longPressMs = 800;

  // Front LEFT cluster (BACK + CONFIRM):
  //   Long press  -> Exit (go back)
  //   Short press -> Random card
  if (mappedInput.wasReleasedAnyOf(HalGPIO::BTN_BACK, HalGPIO::BTN_CONFIRM)) {
    if (mappedInput.getHeldTime() >= longPressMs) {
      if (onGoBack) onGoBack();
      return;
    } else {
      showRandomCard();
      return;
    }
  }

  // Front RIGHT cluster (LEFT + RIGHT):
  //   Long press  -> Exit (go back)
  //   Short press -> Flip card
  if (mappedInput.wasReleasedAnyOf(HalGPIO::BTN_LEFT, HalGPIO::BTN_RIGHT)) {
    if (mappedInput.getHeldTime() >= longPressMs) {
      if (onGoBack) onGoBack();
      return;
    } else {
      toggleFlip();
      return;
    }
  }

  // Side Up / Down -> unchanged (keep existing: navigate sequential cards)
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    showPrevCard();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    showNextCard();
  }
}
