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
  if (!flashcardFiles.empty()) {
    uint32_t r = esp_random() % flashcardFiles.size();
    currentIndex = static_cast<int>(r);
    renderCard(flashcardFiles[currentIndex], true); // Initial card Full Refresh
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
  flashcardFiles.clear();
  // List files in /flashcard
  std::vector<String> files = Storage.listFiles("/flashcard", 500); // Support up to 500 cards
  for (const auto& f : files) {
    // Skip hidden files
    if (f.startsWith(".")) {
      continue;
    }
    if (f.endsWith(".bmp") || f.endsWith(".BMP")) {
      flashcardFiles.push_back("/flashcard/" + std::string(f.c_str()));
    }
  }
  // Sort alphabetically
  std::sort(flashcardFiles.begin(), flashcardFiles.end());
}

void FlashcardActivity::showRandomCard() {
  if (flashcardFiles.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, "No cards in /flashcard");
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
    return;
  }

  uint32_t r = esp_random() % flashcardFiles.size();
  currentIndex = static_cast<int>(r);
  renderCard(flashcardFiles[currentIndex], false);
}

void FlashcardActivity::showNextCard() {
  if (flashcardFiles.empty()) return;
  currentIndex = (currentIndex + 1) % flashcardFiles.size();
  renderCard(flashcardFiles[currentIndex], false);
}

void FlashcardActivity::showPrevCard() {
  if (flashcardFiles.empty()) return;
  currentIndex = (currentIndex - 1 + (int)flashcardFiles.size()) % flashcardFiles.size();
  renderCard(flashcardFiles[currentIndex], false);
}

void FlashcardActivity::renderCard(const std::string& path, bool fullRefresh) {
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

      // Simple UI hints: Up/Down for Seq, Confirm for Shuffle
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Shuffle", "Prev", "Next");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

      renderer.displayBuffer(fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
    } else {
      renderer.clearScreen();
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Invalid BMP");
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Retry", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    }
    file.close();
  } else {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Open Failed");
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Retry", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}

void FlashcardActivity::loop() {
  Activity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (onGoBack) onGoBack();
    return;
  }

  // Confirm -> Shuffle
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    showRandomCard();
  }
  // Up/Left -> Prev
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
           mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    showPrevCard();
  }
  // Down/Right -> Next
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
           mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    showNextCard();
  }
}
