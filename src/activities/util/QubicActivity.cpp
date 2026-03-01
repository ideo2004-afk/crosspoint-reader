#include "QubicActivity.h"
#include <Arduino.h>
#include "fontIds.h"
#include <HalDisplay.h>
#include "components/UITheme.h"

QubicActivity::QubicActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onGoBack)
    : Activity("Qubic", renderer, mappedInput), onGoBack(onGoBack) {}

void QubicActivity::onEnter() {
  renderer.clearScreen();
  engine.reset();
  status = Playing;
  cursorX = 0;
  cursorY = 0;
  cursorZ = 0;
  isAiThinking = false;
  inEscMenu = false;
  escMenuIndex = 0;
  lastAiMove = -1;
  postGameMenuIndex = 0;
  renderBoard(true);
}

void QubicActivity::onExit() {}

void QubicActivity::loop() {
  if (inEscMenu || !isAiThinking) {
      handleInput();
  }
  if (isAiThinking && millis() - aiThinkStartTime > 1200) {
    makeAiMove();
  }
}

void QubicActivity::handleInput() {
  bool moved = false;
  
  // Handle Escape Menu Input
  if (inEscMenu) {
      if (mappedInput.wasReleasedRaw(HalGPIO::BTN_UP)) {
          escMenuIndex = (escMenuIndex > 0) ? escMenuIndex - 1 : 4;
          moved = true;
      } else if (mappedInput.wasReleasedRaw(HalGPIO::BTN_DOWN)) {
          escMenuIndex = (escMenuIndex < 4) ? escMenuIndex + 1 : 0;
          moved = true;
      } else if (mappedInput.wasShortPressedRaw(HalGPIO::BTN_BACK) || mappedInput.wasShortPressedRaw(HalGPIO::BTN_CONFIRM)) {
          if (escMenuIndex == 0) {
              inEscMenu = false; // Resume
              moved = true;
          } else if (escMenuIndex >= 1 && escMenuIndex <= 3) {
              aiDifficulty = escMenuIndex; // L1, L2, L3
              inEscMenu = false;
              onEnter(); // Restart game
              return;
          } else if (escMenuIndex == 4) {
              onGoBack(); // Exit
              return;
          }
      }
      
      if (mappedInput.wasLongPressedRaw(HalGPIO::BTN_BACK) || mappedInput.wasLongPressedRaw(HalGPIO::BTN_CONFIRM)) {
          inEscMenu = false;
          moved = true;
      }
      
      if (moved) renderBoard(false);
      return;
  }

  // Side Buttons: Layer Selection
  if (mappedInput.wasReleasedRaw(HalGPIO::BTN_UP)) {
    cursorZ = (cursorZ > 0) ? cursorZ - 1 : 3;
    moved = true;
  } else if (mappedInput.wasReleasedRaw(HalGPIO::BTN_DOWN)) {
    cursorZ = (cursorZ < 3) ? cursorZ + 1 : 0;
    moved = true;
  }
  
  // Bottom Left Cluster: Circular navigation within layer
  if (status == Playing) {
      if (mappedInput.wasReleasedRaw(HalGPIO::BTN_LEFT)) {
          int idx = cursorY * 4 + cursorX;
          idx = (idx > 0) ? idx - 1 : 15;
          cursorX = idx % 4;
          cursorY = idx / 4;
          moved = true;
      } else if (mappedInput.wasReleasedRaw(HalGPIO::BTN_RIGHT)) {
          int idx = cursorY * 4 + cursorX;
          idx = (idx < 15) ? idx + 1 : 0;
          cursorX = idx % 4;
          cursorY = idx / 4;
          moved = true;
      }
  } else {
      if (mappedInput.wasReleasedRaw(HalGPIO::BTN_LEFT) || mappedInput.wasReleasedRaw(HalGPIO::BTN_RIGHT)) {
          postGameMenuIndex = (postGameMenuIndex == 0) ? 1 : 0;
          moved = true;
      }
  }
  
  // Bottom Right Cluster: Confirm / Menu
  bool confirmPressed = mappedInput.wasShortPressedRaw(HalGPIO::BTN_BACK) || 
                         mappedInput.wasShortPressedRaw(HalGPIO::BTN_CONFIRM);
                         
  bool menuLongPressed = mappedInput.wasLongPressedRaw(HalGPIO::BTN_BACK) || 
                          mappedInput.wasLongPressedRaw(HalGPIO::BTN_CONFIRM);

  if (menuLongPressed) {
      inEscMenu = true;
      escMenuIndex = 0;
      renderBoard(false);
      return;
  }

  if (confirmPressed) {
      if (status != Playing) {
          if (postGameMenuIndex == 0) {
              onEnter();
          } else {
              onGoBack();
          }
          return;
      } else {
          int idx = cursorZ * 16 + cursorY * 4 + cursorX;
          if (engine.makeMove(idx, QubicEngine::Human)) {
            renderBoard(false);
            QubicEngine::Player winner = engine.checkWinner();
            if (winner == QubicEngine::Human) {
              status = Won;
              postGameMenuIndex = 0;
            } else if (engine.isFull()) {
              status = Draw;
              postGameMenuIndex = 0;
            } else {
              isAiThinking = true;
              aiThinkStartTime = millis();
            }
            renderBoard(false);
          }
      }
  }

  if (moved) {
    renderBoard(false);
  }
}

void QubicActivity::makeAiMove() {
  int bestMove = engine.getBestMove(aiDifficulty);
  if (bestMove != -1) {
    engine.makeMove(bestMove, QubicEngine::AI);
    lastAiMove = bestMove;
  }
  isAiThinking = false;
  
  QubicEngine::Player winner = engine.checkWinner();
  if (winner == QubicEngine::AI) {
    status = Lost;
    postGameMenuIndex = 0;
  } else if (engine.isFull()) {
    status = Draw;
    postGameMenuIndex = 0;
  }
  renderBoard(false);
}

QubicActivity::Point QubicActivity::getIsometricPoint(float x, float y, int z) {
  int screenWidth = renderer.getScreenWidth();
  int baseX = screenWidth / 2; 
  int baseY = 160 + (z * 150); 
  
  float isoScaleX = 36.0f;
  float isoScaleY = 16.0f;
  
  int isoX = baseX + (int)((x - y) * isoScaleX);
  int isoY = baseY + (int)((x + y) * isoScaleY);
  
  return { isoX, isoY };
}

void QubicActivity::renderBoard(bool fullRefresh) {
  renderer.clearScreen(); 

  // 1. Stylized Header
  renderer.drawText(UI_12_FONT_ID, 20, 20, "3D Tic-Tac-Toe", true, EpdFontFamily::BOLD);
  renderer.fillRect(0, 60, renderer.getScreenWidth(), 3, true); // Thick line

  std::string diffLabel = (aiDifficulty == 1) ? "L1 (Easy)" : (aiDifficulty == 2) ? "L2 (Medium)" : "L3 (Hard)";
  renderer.drawText(SMALL_FONT_ID, 20, 80, diffLabel.c_str(), true);


  // 2. Draw Layer Labels (removed)

  // 3. Draw Grid
  for (int z = 0; z < 4; z++) {
    for (int i = 0; i <= 4; i++) {
      Point p1 = getIsometricPoint(0, (float)i, z);
      Point p2 = getIsometricPoint(4, (float)i, z);
      renderer.drawLine(p1.x, p1.y, p2.x, p2.y);
      
      Point p3 = getIsometricPoint((float)i, 0, z);
      Point p4 = getIsometricPoint((float)i, 4, z);
      renderer.drawLine(p3.x, p3.y, p4.x, p4.y);
    }

    // Pieces
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        uint8_t p = engine.getAt(x, y, z);
        if (p == 0) continue;
        
        Point pt = getIsometricPoint(x + 0.5f, y + 0.5f, z);
        if (p == QubicEngine::Human) { // '+' Cross
          renderer.drawLine(pt.x, pt.y - 8, pt.x, pt.y + 8, 2, true);
          renderer.drawLine(pt.x - 14, pt.y, pt.x + 14, pt.y, 2, true);
        } else if (p == QubicEngine::AI) { // Octagon "O"
          int th = ((z * 16 + y * 4 + x) == lastAiMove) ? 4 : 2;
          renderer.drawLine(pt.x - 6, pt.y - 8, pt.x + 6, pt.y - 8, th, true); // Top
          renderer.drawLine(pt.x + 6, pt.y - 8, pt.x + 12, pt.y - 2, th, true); // TR
          renderer.drawLine(pt.x + 12, pt.y - 2, pt.x + 12, pt.y + 2, th, true); // R
          renderer.drawLine(pt.x + 12, pt.y + 2, pt.x + 6, pt.y + 8, th, true); // BR
          renderer.drawLine(pt.x + 6, pt.y + 8, pt.x - 6, pt.y + 8, th, true); // B
          renderer.drawLine(pt.x - 6, pt.y + 8, pt.x - 12, pt.y + 2, th, true); // BL
          renderer.drawLine(pt.x - 12, pt.y + 2, pt.x - 12, pt.y - 2, th, true); // L
          renderer.drawLine(pt.x - 12, pt.y - 2, pt.x - 6, pt.y - 8, th, true); // TL
        }
      }
    }
  }

  // 4. Draw Cursor Frame
  if (status == Playing && !inEscMenu) {
      Point c1 = getIsometricPoint(cursorX, cursorY, cursorZ);
      Point c2 = getIsometricPoint(cursorX + 1.0f, cursorY, cursorZ);
      Point c3 = getIsometricPoint(cursorX + 1.0f, cursorY + 1.0f, cursorZ);
      Point c4 = getIsometricPoint(cursorX, cursorY + 1.0f, cursorZ);
      
      renderer.drawLine(c1.x, c1.y, c2.x, c2.y, 3, true);
      renderer.drawLine(c2.x, c2.y, c3.x, c3.y, 3, true);
      renderer.drawLine(c3.x, c3.y, c4.x, c4.y, 3, true);
      renderer.drawLine(c4.x, c4.y, c1.x, c1.y, 3, true);
  }

  // 5. Status at bottom
  if (status != Playing) {
      if (status == Won) {
          renderer.drawText(UI_12_FONT_ID, 20, 750, "VICTORY!", true, EpdFontFamily::BOLD);
      } else if (status == Lost) {
          renderer.drawText(UI_12_FONT_ID, 20, 750, "DEFEAT.", true, EpdFontFamily::BOLD);
      } else if (status == Draw) {
          renderer.drawText(UI_12_FONT_ID, 20, 750, "DRAW.", true, EpdFontFamily::BOLD);
      }
      
      // Post-game Pills: New / Exit
      for (int i=0; i<2; i++) {
          int bx = 180 + (i * 90);
          int by = 744;
          if (postGameMenuIndex == i) {
              renderer.fillRoundedRect(bx - 10, by - 5, 80, 40, 10, true, true, false, false, Color::Black);
              renderer.drawText(UI_12_FONT_ID, bx + 10, by + 10, (i == 0) ? "New" : "Exit", false, EpdFontFamily::REGULAR);
          } else {
              renderer.drawRoundedRect(bx - 10, by - 5, 80, 40, 2, 10, true, true, false, false, true);
              renderer.drawText(UI_12_FONT_ID, bx + 10, by + 10, (i == 0) ? "New" : "Exit", true, EpdFontFamily::REGULAR);
          }
      }
      
  } else if (isAiThinking) {
      renderer.drawText(UI_10_FONT_ID, 340, 750, "AI Thinking...");
  }

  // 6. Escape Menu Overlay
  if (inEscMenu) {
      renderEscMenu();
  }

  renderer.displayBuffer();
}

void QubicActivity::renderEscMenu() {
    int sw = renderer.getScreenWidth();
    int sh = renderer.getScreenHeight();
    int mw = 320;
    int mh = 330;
    int mx = (sw - mw) / 2;
    int my = (sh - mh) / 2;

    renderer.fillRect(mx - 4, my - 4, mw + 8, mh + 8, true); // Border
    renderer.fillRect(mx, my, mw, mh, false); // Background (white)
    
    renderer.drawText(UI_12_FONT_ID, mx + 20, my + 20, "Game Menu", true, EpdFontFamily::BOLD);
    
    // Options
    const char* options[] = {"Resume", "L1 (Easy)", "L2 (Medium)", "L3 (Hard)", "Exit Game"};
    for (int i = 0; i < 5; i++) {
        int ry = my + 65 + (i * 50);
        if (escMenuIndex == i) {
            renderer.fillRect(mx + 10, ry - 5, mw - 20, 40, true);
        }
        
        bool blackText = (escMenuIndex != i);
        renderer.drawText(UI_12_FONT_ID, mx + 20, ry + 2, options[i], blackText, EpdFontFamily::REGULAR);
    }
}
