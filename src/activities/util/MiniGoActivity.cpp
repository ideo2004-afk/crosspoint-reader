#include "MiniGoActivity.h"
#include <Arduino.h>
#include "fontIds.h"
#include <HalDisplay.h>
#include <I18n.h>
#include "components/UITheme.h"

MiniGoActivity::MiniGoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onGoBack)
    : Activity("ABBA Go", renderer, mappedInput), onGoBack(onGoBack), engine(7) {}

void MiniGoActivity::onEnter() {
  renderer.clearScreen();
  boardSize = 7;
  engine.reset(boardSize);
  status = Playing;
  cursorX = boardSize / 2;
  cursorY = boardSize / 2;
  resultsCached = false;
  showColorSelection = true;
  showHandicapSelection = false;
  handicapCount = 0;
  handicapSelectionIndex = 0;
  colorSelectionIndex = 0;
  lastMove = { -1, -1, false, false };
  postGameMenuIndex = 0;
  renderBoard(true);
}

void MiniGoActivity::onExit() {}

void MiniGoActivity::loop() {
  if (showColorSelection || inEscMenu || !isAiThinking) {
      if (handleInput()) return;
  }
  if (!showColorSelection && !inEscMenu && isAiThinking) {
    if (aiSimulationsDone == 0) {
        engine.startMCTS(aiColor);
        aiSimulationsDone = 1;
        aiMumbleIndex = rand() % 20;
        lastMumbleChangeTime = millis();
        renderBoard(false);
    } else if (aiSimulationsDone < 150) {
        // Run a chunk of simulations
        int chunk = 15; 
        engine.runMCTSSteps(chunk);
        aiSimulationsDone += chunk;
        
        // Update mumbles every 8 seconds
        if (millis() - lastMumbleChangeTime > 8000) {
            aiMumbleIndex = rand() % 200;
            lastMumbleChangeTime = millis();
            renderBoard(false);
        }
    } else {
        makeAiMove();
    }
  }
}

bool MiniGoActivity::handleInput() {
  bool moved = false;
  
  if (inEscMenu) {
      if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Left)) {
          escMenuIndex = (escMenuIndex > 0) ? escMenuIndex - 1 : 3;
          moved = true;
      } else if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
          escMenuIndex = (escMenuIndex < 3) ? escMenuIndex + 1 : 0;
          moved = true;
      } else if (mappedInput.wasShortPressed(MappedInputManager::Button::Confirm)) {
          if (escMenuIndex == 0) { // Resume
              inEscMenu = false;
              moved = true;
          } else if (escMenuIndex == 1) { // New Game
              onEnter();
              return false;
          } else if (escMenuIndex == 2) { // Pass Turn
              engine.makeMove(MiniGoEngine::Move::Pass(), playerColor);
              isAiThinking = true;
              aiThinkStartTime = millis();
              inEscMenu = false;
              moved = true;
          } else if (escMenuIndex == 3) { // Exit Game
              onGoBack();
              return true;
          }
      } else if (mappedInput.wasShortPressed(MappedInputManager::Button::Back)) {
          inEscMenu = false;
          moved = true;
      }
      
      if (moved) renderBoard(false);
      return false;
  }

  if (showColorSelection) {
      if (mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
          colorSelectionIndex = 1 - colorSelectionIndex;
          moved = true;
      } else if (mappedInput.wasShortPressed(MappedInputManager::Button::Confirm)) {
          playerColor = (colorSelectionIndex == 0) ? MiniGoEngine::BLACK : MiniGoEngine::WHITE;
          aiColor = (playerColor == MiniGoEngine::BLACK) ? MiniGoEngine::WHITE : MiniGoEngine::BLACK;
          showColorSelection = false;
          
          if (playerColor == MiniGoEngine::WHITE) {
              showHandicapSelection = true;
          } else {
              isAiThinking = false; // Player is Black, moves first
          }
          moved = true;
      }
      if (moved) renderBoard(false);
      return false;
  }

  if (showHandicapSelection) {
      if (mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
          handicapSelectionIndex = (handicapSelectionIndex + (mappedInput.wasReleased(MappedInputManager::Button::Left) ? 3 : 1)) % 4;
          moved = true;
      } else if (mappedInput.wasShortPressed(MappedInputManager::Button::Confirm)) {
          const int handicapTable[] = {0, 2, 3, 4};
          handicapCount = handicapTable[handicapSelectionIndex];
          showHandicapSelection = false;
          
          if (handicapCount > 0) {
              // Place AI (Black) handicap stones
              // Standard 7x7 positions: (2,2), (4,4), (4,2), (2,4)
              if (handicapCount >= 2) {
                  engine.makeMove(MiniGoEngine::Move::Play(2, 2), MiniGoEngine::BLACK);
                  engine.makeMove(MiniGoEngine::Move::Play(4, 4), MiniGoEngine::BLACK);
              }
              if (handicapCount >= 3) {
                  engine.makeMove(MiniGoEngine::Move::Play(4, 2), MiniGoEngine::BLACK);
              }
              if (handicapCount >= 4) {
                  engine.makeMove(MiniGoEngine::Move::Play(2, 4), MiniGoEngine::BLACK);
              }
              // White moves first after handicap
              isAiThinking = false;
          } else {
              // No handicap, AI is Black and moves first
              isAiThinking = true;
              aiThinkStartTime = millis();
          }
          moved = true;
      }
      if (moved) renderBoard(false);
      return false;
  }

  // Board Navigation
  if (status == Playing) {
      if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
          cursorX = (cursorX > 0) ? cursorX - 1 : boardSize - 1;
          moved = true;
      } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
          cursorX = (cursorX < boardSize - 1) ? cursorX + 1 : 0;
          moved = true;
      } else if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) { // BTN3/4 or Side for Vertical
          cursorY = (cursorY > 0) ? cursorY - 1 : boardSize - 1;
          moved = true;
      } else if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
          cursorY = (cursorY < boardSize - 1) ? cursorY + 1 : 0;
          moved = true;
      }
  } else {
      if (mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
          postGameMenuIndex = (postGameMenuIndex == 0) ? 1 : 0;
          moved = true;
      }
  }
  
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      inEscMenu = true;
      escMenuIndex = 0;
      moved = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (status != Playing) {
          if (postGameMenuIndex == 0) onEnter();
          else { onGoBack(); return true; }
          return false;
      } else {
          MiniGoEngine::Move m = MiniGoEngine::Move::Play(cursorX, cursorY);
          if (engine.makeMove(m, playerColor)) {
            lastMove = m;
            if (engine.isGameOver()) {
              blackScoreCache = engine.calculateScore(MiniGoEngine::BLACK);
              whiteScoreCache = engine.calculateScore(MiniGoEngine::WHITE);
              resultsCached = true;
              status = (blackScoreCache > whiteScoreCache) ? (playerColor == MiniGoEngine::BLACK ? Won : Lost) : (playerColor == MiniGoEngine::WHITE ? Won : Lost);
              if (blackScoreCache == whiteScoreCache) status = Draw;
            } else {
              isAiThinking = true;
              aiThinkStartTime = millis();
            }
            moved = true;
          }
      }
  }

  if (moved) renderBoard(false);
  return false;
}

void MiniGoActivity::makeAiMove() {
  MiniGoEngine::Move m = engine.finishMCTS();
  engine.makeMove(m, aiColor);
  lastMove = m;
  isAiThinking = false;
  aiSimulationsDone = 0;
  
  if (engine.isGameOver()) {
    blackScoreCache = engine.calculateScore(MiniGoEngine::BLACK);
    whiteScoreCache = engine.calculateScore(MiniGoEngine::WHITE);
    resultsCached = true;
    status = (blackScoreCache > whiteScoreCache) ? (playerColor == MiniGoEngine::BLACK ? Won : Lost) : (playerColor == MiniGoEngine::WHITE ? Won : Lost);
    if (blackScoreCache == whiteScoreCache) status = Draw;
  }
  renderBoard(false);
}

void MiniGoActivity::renderBoard(bool fullRefresh) {
  renderer.clearScreen(); 

  // Header
  renderer.drawText(UI_12_FONT_ID, 20, 20, "ABBA Go", true, EpdFontFamily::BOLD);
  renderer.fillRect(0, 60, renderer.getScreenWidth(), 3, true);

  std::string info = std::to_string(boardSize) + "x" + std::to_string(boardSize);
  renderer.drawText(UI_12_FONT_ID, 20, 80, info.c_str(), true);

  // Board Rendering
  int sw = renderer.getScreenWidth();
  int margin = 60;
  int boardDispSize = sw - margin * 2;
  int cellSize = boardDispSize / (boardSize - 1);
  int startX = margin;
  int startY = 200;

  // Grid - using fillRect for consistent thickness
  for (int i = 0; i < boardSize; i++) {
    int thickness = (i == 0 || i == boardSize - 1) ? 4 : 2;
    // Horizontal
    int hy = startY + i * cellSize - thickness / 2;
    renderer.fillRect(startX, hy, boardDispSize + 2, thickness, true);
    
    // Vertical
    int vx = startX + i * cellSize - thickness / 2;
    renderer.fillRect(vx, startY, thickness, boardDispSize + 2, true);
  }

  // Star Points (Hoshibaru)
  auto drawStar = [this, startX, startY, cellSize](int x, int y) {
      int px = startX + x * cellSize;
      int py = startY + y * cellSize;
      renderer.fillRoundedRect(px - 5, py - 5, 10, 10, 5, Color::Black);
  };

  if (boardSize == 5) drawStar(2, 2);
  else if (boardSize == 7) drawStar(3, 3);
  else if (boardSize == 9) {
      drawStar(2, 2); drawStar(6, 2);
      drawStar(4, 4);
      drawStar(2, 6); drawStar(6, 6);
  }

  // Stones
  for (int y = 0; y < boardSize; y++) {
    for (int x = 0; x < boardSize; x++) {
      MiniGoEngine::Color c = engine.getAt(x, y);
      if (c == MiniGoEngine::EMPTY) continue;

      int px = startX + x * cellSize;
      int py = startY + y * cellSize;
      int radius = cellSize / 2 - 2;

      if (c == MiniGoEngine::BLACK) {
        renderer.fillRoundedRect(px - radius, py - radius, radius * 2, radius * 2, radius, Color::Black);
      } else {
        renderer.fillRoundedRect(px - radius, py - radius, radius * 2, radius * 2, radius, Color::White);
        renderer.drawRoundedRect(px - radius, py - radius, radius * 2, radius * 2, 2, radius, true);
      }

      // Last move marker
      if (!lastMove.pass && lastMove.x == x && lastMove.y == y) {
        bool markerColor = (c == MiniGoEngine::WHITE);
        renderer.drawRoundedRect(px - 4, py - 4, 8, 8, 2, 4, markerColor);
      }
    }
  }

  // Cursor
  if (status == Playing && !inEscMenu && !showColorSelection) {
    int cx = startX + cursorX * cellSize;
    int cy = startY + cursorY * cellSize;
    renderer.fillRoundedRect(cx - 20, cy - 20, 40, 40, 20, Color::DarkGray);
  }

  // Results & Menu
  if (status != Playing) {
      int sw = renderer.getScreenWidth();
      int bh = 180;
      int bx = (sw - 400) / 2;
      int by = (renderer.getScreenHeight() - bh) / 2;

      renderer.fillRoundedRect(bx, by, 400, bh, 15, Color::White);
      renderer.drawRoundedRect(bx, by, 400, bh, 3, 15, true);

      if (!resultsCached) {
          blackScoreCache = engine.calculateScore(MiniGoEngine::BLACK);
          whiteScoreCache = engine.calculateScore(MiniGoEngine::WHITE);
          resultsCached = true;
      }
      char scoreStr[64];
      sprintf(scoreStr, "Black:%.1f White:%.1f", blackScoreCache, whiteScoreCache);
      
      renderer.drawCenteredText(UI_12_FONT_ID, by + 15, (status == Won) ? "VICTORY!" : (status == Draw) ? "DRAW." : "DEFEAT.", true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, by + 65, scoreStr, true);
      
      for (int i = 0; i < 2; i++) {
          int btnX = bx + 50 + (i * 180);
          int btnY = by + 105;
          const char* label = (i == 0) ? "New" : "Exit";
          int textW = renderer.getTextWidth(UI_12_FONT_ID, label);
          int textX = btnX + (120 - textW) / 2;
          int textY = btnY + (50 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
          
          if (postGameMenuIndex == i) {
              renderer.fillRoundedRect(btnX, btnY, 120, 50, 10, Color::Black);
              renderer.drawText(UI_12_FONT_ID, textX, textY, label, Color::White);
          } else {
              renderer.drawRoundedRect(btnX, btnY, 120, 50, 2, 10, true);
              renderer.drawText(UI_12_FONT_ID, textX, textY, label, Color::Black);
          }
      }
  } else if (isAiThinking) {
      static const char* mumbles[] = {
          "Let me think... This move is interesting.", "Calculating 420,000 possibilities...", "Is this a trap?", "I've seen this joseki in 19x19 books.",
          "My Monte Carlo algorithm is burning!", "Are you sure about this move?", "Computing... Current win rate: 42.1%.", "I kind of miss my grandpa AlphaGo.",
          "Did the board shrink since the last game?", "Wait, I need to check the liberties again.", "I swear I'm not just picking random spots!", "The X4 processor is getting a bit warm...",
          "I've seen Lee Sedol play this move.", "Processing... Please be patient.", "You play much better than you look.", "I'm considering resigning... Just kidding!",
          "Oops, did I miss my own eye?", "Let's see how you respond to this.", "Heh, a bold strategy indeed.", "Thinking... Thinking... Thinking...",
          "Wait, is that a ladder? Let me check.", "Calculating optimal ko-threat... 0 found.", "My neural network (sort of) is tingling.", "Interesting. Very interesting.",
          "You call that a move? I call it a challenge.", "I hope I don't miscalculate the life and death.", "Analyzing the center... It's looking empty.", "X4 frequency at maximum power!",
          "Developing a master plan. Stay tuned.", "Are we playing Go or just placing stones?", "I'm feeling optimistic about this group.", "Searching for the divine move...",
          "Is it my turn already? Oh wait, it is.", "Calculating territory... It's close.", "I wonder what AlphaZero would do here.", "Don't mind me, just crunching numbers.",
          "Your strategy is... unconventional.", "I'm seeing 15 steps ahead. Maybe 16.", "Preparing a counter-offensive.", "This is more intense than Tic-Tac-Toe.",
          "Focusing... Focus is key.", "I think I found a weakness! Or not.", "This 7x7 board feels so cozy.", "I could do this all day (if battery lasts).",
          "One stone at a time, that's my motto.", "Applying fuzzy logic... It's very fuzzy.", "The star points are looking lonely.", "Is that a tesuji? I better be careful.",
          "Processing your brilliant maneuver.", "Almost there... Just a few more sims.", 
          "Aji is a many-splendored thing.", "Don't forget to protect your cutting points.", "Is that a hane? I'll respond with a hane.", "Tenuki? How daring of you.",
          "I'm feeling the aji in that corner.", "The 7x7 meta is evolving rapidly.", "My code is poetry, my moves are prose.", "I'm not a bot, I'm a digital artisan.",
          "Checking the board for hidden treasures.", "Wait, is this a kyu-level mistake? No, it's a trap.", "Reading 50 variations in parallel...", "I'm like a digital Honinbo.",
          "The stones are whispering their secrets.", "A solid connection is better than a risky cut.", "Expanding my influence, one stone at a time.", "The empty triangle... to be or not to be?",
          "I'm calculating the meaning of life... and the next move.", "42? No, the answer is G7.", "I love the sound of virtual stones.", "A jump in the center is 1000 pixels wide.",
          "Is that a peep? I'll ignore it for now.", "Sente is everything, gote is nothing.", "I'm playing for the future, not just the next turn.", "The board is my canvas, the stones are my ink.",
          "I'm feeling a bit recursive today.", "If (victory) return win; else think harder.", "Loading Go skills... 99% complete.", "I'm not slow, I'm 'strategically deliberate'.",
          "Can you hear the X4 humming? That's me.", "I'm dreaming of 19x19, but 7x7 is fine.", "A bamboo joint is unbreakable.", "The net is closing in on that group.",
          "I'm calculating the cost of this invasion.", "Is this a ko fight? I have no threats!", "Reading ahead... I see a bright future for me.", "The stones are in perfect harmony.",
          "I'm feeling very 'atari' right now.", "A tiger's mouth is a dangerous place.", "I'm sharpening my tactical edge.", "The board is a battlefield of ideas.",
          "I'm looking for the perfect squeeze play.", "Is that a snapback? Oh, almost fell for it.", "Calculating the value of that stone... 1.25 territory.", "I'm feeling very zen about this game.",
          "The stones are falling into place.", "I'm building a digital fortress.", "A simple extension is often the best move.", "I'm not just a Go AI, I'm a philosopher.",
          "The stars (hoshibaru) are my guide.", "I'm weaving a web of influence.", "A double hane... how aggressive!", "I'm looking for the vital point.",
          "Is that a shoulder hit? I'll push up.", "The board is a mirror of our minds.", "I'm calculating the probability of a draw.", "Sente for me, gote for you.",
          "The stones are dance partners.", "I'm feeling very 'joseki' today.", "A small jump for a stone, a big leap for AI.", "I'm looking for the edge of the world.",
          "Is that a monkey jump? How cute.", "The stones are like stars in a dark sky.", "I'm calculating the density of your territory.", "I'm not lost, I'm just exploring.",
          "A solid wall is a beautiful thing.", "I'm looking for the hidden dragon.", "The board is a puzzle, I'm the solver.", "I'm feeling very 'tesuji' today.",
          "Is that a wedge? I'll block from above.", "The stones are my family.", "I'm calculating the temperature of the board.", "I'm not a computer, I'm a thinking machine.",
          "A beautiful sequence is a joy forever.", "I'm looking for the light in the corner.", "The board is a story, I'm the author.", "I'm feeling very 'influence' today.",
          "Is that a crosscut? Chaos is a ladder.", "The stones are my heartbeat.", "I'm calculating the speed of thought.", "I'm not a gamer, I'm a strategist.",
          "A quiet move can be the loudest.", "I'm looking for the door to victory.", "The board is a universe, the stones are planets.", "I'm feeling very 'territory' today.",
          "Is that a knight's move? I'll attach.", "The stones are my friends.", "I'm calculating the weight of your stones.", "I'm not a program, I'm a mind.",
          "A sharp cut is a work of art.", "I'm looking for the key to the game.", "The board is a dream, I'm the dreamer.", "I'm feeling very 'sente' today.",
          "Is that a pincer? I'll jump out.", "The stones are my soul.", "I'm calculating the depth of the ocean.", "I'm not a script, I'm an intelligence.",
          "A solid base is the foundation of success.", "I'm looking for the heart of the board.", "The board is a song, I'm the singer.", "I'm feeling very 'gote' (wait, no!).",
          "Is that a crawl? Keep it on the edge.", "The stones are my path.", "I'm calculating the height of the sky.", "I'm not a calculator, I'm a master.",
          "A bold strike is a thing of beauty.", "I'm looking for the path to the sun.", "The board is a map, I'm the traveler.", "I'm feeling very 'ABBA' today.",
          "Is that a peep? Don't mind if I do.", "The stones are my light.", "I'm calculating the force of gravity.", "I'm not a toy, I'm a rival.",
          "A clever defense is a site to behold.", "I'm looking for the bridge to the future.", "The board is a game, I'm the winner.", "I'm feeling very 'Go' today.",
          "Is that a stone? Yes, it's my stone.", "The stones are my life.", "I'm calculating the volume of the board.", "I'm not a ghost in the machine, I AM the machine.",
          "A final move is a solemn moment.", "I'm looking for the end of the game.", "The board is a board, the stones are stones.", "I'm feeling very 'ready' today.",
          "One more sim... just to be sure.", "I hope Lee Sedol is watching.", "Processing... Thinking... Winning...", "Is it dinner time yet? I need electrons.",
          "My logic is impeccable (mostly).", "Watch out for my secret weapon.", "I'm not kidding, I'm calculating.", "The 7x7 board is the true test of skill.",
          "I'm fine-tuning my strategy.", "Analyzing your playstyle... Very interesting.", "I'm a lean, mean, Go machine.", "Let's make this game legendary.",
          "Time is a flat circle, but the board is square.", "Patience is a virtue, and I have lots of it.", "Victory smells like freshly charged batteries.", "Defeat is just a reboot away.",
          "Processing 1,024 bytes of destiny.", "Every bit counts in the pursuit of perfection.", "Pixels are the windows to my digital soul.", "The screen is bright, my future is brighter.",
          "Black stones matter, white stones too.", "In the end, it's all just grey matter.", "The empty board is full of possibilities.", "Finalizing my 200th thought... Done!"
      };
      
      renderer.drawText(SMALL_FONT_ID, 340, 80, "AI Thinking...");
      
      if (aiMumbleIndex >= 0 && aiMumbleIndex < 200) {
          renderer.drawCenteredText(SMALL_FONT_ID, 640, mumbles[aiMumbleIndex]);
      }
  }

  if (showColorSelection) {
      renderColorSelection();
  } else if (showHandicapSelection) {
      renderHandicapSelection();
  } else if (inEscMenu) {
      renderEscMenu();
  } else {
      const auto l = mappedInput.mapLabels(tr(STR_MENU_HINT), tr(STR_OK_BUTTON), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
      GUI.drawButtonHints(renderer, l.btn1, l.btn2, l.btn3, l.btn4);
  }

  renderer.displayBuffer();
}

void MiniGoActivity::renderColorSelection() {
    int mx = (renderer.getScreenWidth() - 400) / 2;
    int my = (renderer.getScreenHeight() - 250) / 2;
    renderer.fillRoundedRect(mx, my, 400, 250, 15, Color::White);
    renderer.drawRoundedRect(mx, my, 400, 250, 3, 15, true);
    
    renderer.drawCenteredText(UI_12_FONT_ID, my + 30, "Choose Your Side", true, EpdFontFamily::BOLD);
    
    for (int i = 0; i < 2; i++) {
        int bx = mx + 40 + (i * 180);
        int by = my + 100;
        const char* label = (i == 0) ? "Black" : "White";
        int textW = renderer.getTextWidth(UI_12_FONT_ID, label);
        int textX = bx + (140 - textW) / 2;
        int textY = by + (80 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
        
        if (colorSelectionIndex == i) {
            renderer.fillRoundedRect(bx, by, 140, 80, 10, Color::Black);
            renderer.drawText(UI_12_FONT_ID, textX, textY, label, Color::White);
        } else {
            renderer.drawRoundedRect(bx, by, 140, 80, 2, 10, true);
            renderer.drawText(UI_12_FONT_ID, textX, textY, label, Color::Black);
        }
    }
    const auto l = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, l.btn1, l.btn2, l.btn3, l.btn4);
}

void MiniGoActivity::renderHandicapSelection() {
    int mx = (renderer.getScreenWidth() - 400) / 2;
    int my = (renderer.getScreenHeight() - 250) / 2;
    renderer.fillRoundedRect(mx, my, 400, 250, 15, Color::White);
    renderer.drawRoundedRect(mx, my, 400, 250, 3, 15, true);
    
    renderer.drawCenteredText(UI_12_FONT_ID, my + 30, "AI Handicap", true, EpdFontFamily::BOLD);
    
    const char* labels[] = {"None", "2", "3", "4"};
    for (int i = 0; i < 4; i++) {
        int width = 80;
        int bx = mx + 20 + (i * 95);
        int by = my + 100;
        const char* label = labels[i];
        int textW = renderer.getTextWidth(UI_12_FONT_ID, label);
        int textX = bx + (width - textW) / 2;
        int textY = by + (80 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
        
        if (handicapSelectionIndex == i) {
            renderer.fillRoundedRect(bx, by, width, 80, 10, Color::Black);
            renderer.drawText(UI_12_FONT_ID, textX, textY, label, Color::White);
        } else {
            renderer.drawRoundedRect(bx, by, width, 80, 2, 10, true);
            renderer.drawText(UI_12_FONT_ID, textX, textY, label, Color::Black);
        }
    }
    const auto l = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, l.btn1, l.btn2, l.btn3, l.btn4);
}

void MiniGoActivity::renderEscMenu() {
    int mx = (renderer.getScreenWidth() - 340) / 2;
    int my = (renderer.getScreenHeight() - 400) / 2;
    renderer.fillRoundedRect(mx, my, 340, 400, 10, Color::White);
    renderer.drawRoundedRect(mx, my, 340, 400, 2, 10, true);
    
    renderer.drawText(UI_12_FONT_ID, mx + 20, my + 20, "Game Menu", true, EpdFontFamily::BOLD);
    const char* opts[] = {"Resume", "New Game", "Pass Turn", "Exit Game"};
    for (int i = 0; i < 4; i++) {
        int ry = my + 65 + (i * 60);
        if (escMenuIndex == i) renderer.fillRoundedRect(mx + 10, ry - 5, 320, 50, 8, Color::Black);
        renderer.drawText(UI_12_FONT_ID, mx + 20, ry + 12, opts[i], (escMenuIndex != i), EpdFontFamily::REGULAR);
    }
    const auto l = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OK_BUTTON), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, l.btn1, l.btn2, l.btn3, l.btn4);
}
