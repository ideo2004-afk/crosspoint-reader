#pragma once

#include "../Activity.h"
#include "MappedInputManager.h"
#include "MiniGoEngine.h"

class MiniGoActivity final : public Activity {
 public:
  MiniGoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onGoBack);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  std::function<void()> onGoBack;
  MiniGoEngine engine;
  
  int cursorX = 0;
  int cursorY = 0;
  int boardSize = 5;
  
  bool isAiThinking = false;
  unsigned long aiThinkStartTime = 0;
  
  enum GameStatus { Playing, Won, Lost, Draw };
  GameStatus status = Playing;

  MiniGoEngine::Move lastMove = { -1, -1, false, false };
  int postGameMenuIndex = 0;

  bool inEscMenu = false;
  int escMenuIndex = 0; // 0: Resume, 1: size 5, 2: size 7, 3: size 9, 4: Pass, 5: Exit

  bool showColorSelection = true;
  int colorSelectionIndex = 0; // 0: Black, 1: White
  MiniGoEngine::Color playerColor = MiniGoEngine::BLACK;
  MiniGoEngine::Color aiColor = MiniGoEngine::WHITE;
  int aiSimulationsDone = 0;

  int aiMumbleIndex = -1;
  unsigned long lastMumbleChangeTime = 0;

  bool showHandicapSelection = false;
  int handicapSelectionIndex = 0; // 0: None, 1: 2 stones, 2: 3 stones, 3: 4 stones
  int handicapCount = 0;

  float blackScoreCache = 0;
  float whiteScoreCache = 0;
  bool resultsCached = false;

  void renderBoard(bool fullRefresh = false);
  void renderEscMenu();
  void renderColorSelection();
  void renderHandicapSelection();
  bool handleInput();
  void makeAiMove();
};
