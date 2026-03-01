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
    std::string sideB;
    int viewCount = 0;
    bool learned = false;
  };

  std::function<void()> onGoBack;
  std::vector<FlashCardPair> cards;
  int currentIndex = -1;
  bool isShowingBack = false;

  // Deck management
  bool inDeckSelection = true;
  std::vector<std::string> decks;
  int deckSelectedIndex = 0;
  std::string selectedDeckName;

  // Sub-menu management
  bool inSubMenu = false;
  int subMenuSelectedIndex = 0;

  void loadDecks();
  void renderDeckMenu(bool fullRefresh = false);
  void renderSubMenu();
  void loadFileList(const std::string& deckName);
  void loadProgress();
  void saveProgress();
  void showRandomCard();
  void showNextCard();
  void showPrevCard();
  void showFirstCard();
  void jumpCards(int delta);
  void toggleFlip();
  void markAsLearned();
  void renderCard(int index, bool isBack, bool fullRefresh = false);
};
