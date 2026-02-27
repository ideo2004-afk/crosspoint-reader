#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "MappedInputManager.h"

class FlashcardActivity final : public Activity {
 public:
  FlashcardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onGoBack);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  std::function<void()> onGoBack;
  std::vector<std::string> flashcardFiles;
  int currentIndex = -1;

  void loadFileList();
  void showRandomCard();
  void showNextCard();
  void showPrevCard();
  void renderCard(const std::string& path, bool fullRefresh = false);
};
