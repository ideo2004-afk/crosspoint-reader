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
  struct FlashCardPair {
    std::string sideA;
    std::string sideB; // Empty if no back side
  };

  std::function<void()> onGoBack;
  std::vector<FlashCardPair> cards;
  int currentIndex = -1;
  bool isShowingBack = false;

  void loadFileList();
  void showRandomCard();
  void showNextCard();
  void showPrevCard();
  void toggleFlip();
  void renderCard(int index, bool isBack, bool fullRefresh = false);
};
