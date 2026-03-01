#pragma once

#include "../Activity.h"
#include "MappedInputManager.h"
#include "QubicEngine.h"

class QubicActivity final : public Activity {
 public:
  struct Point { int x, y; };

  QubicActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onGoBack);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  std::function<void()> onGoBack;
  QubicEngine engine;
  
  int cursorX = 0;
  int cursorY = 0;
  int cursorZ = 0;
  
  bool isAiThinking = false;
  unsigned long aiThinkStartTime = 0;
  
  int aiDifficulty = 1; // 1 = Easy, 2 = Medium, 3 = Hard
  
  enum GameStatus { Playing, Won, Lost, Draw };
  GameStatus status = Playing;

  int lastAiMove = -1;
  int postGameMenuIndex = 0;

  bool inEscMenu = false;
  int escMenuIndex = 0; // 0: Restart, 1: AI Level, 2: Exit

  void renderBoard(bool fullRefresh = false);
  void renderEscMenu();
  void handleInput();
  void makeAiMove();
  
  // Coordinate helpers for Isometric rendering
  Point getIsometricPoint(float x, float y, int z);
};
