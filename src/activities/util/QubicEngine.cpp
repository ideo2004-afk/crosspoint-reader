#include "QubicEngine.h"
#include <Arduino.h>
#include <algorithm>

std::vector<QubicEngine::Line> QubicEngine::winningLines;
bool QubicEngine::linesInitialized = false;

QubicEngine::QubicEngine() {
  reset();
  if (!linesInitialized) {
    initializeLines();
  }
}

void QubicEngine::reset() {
  for (int i = 0; i < 64; i++) board[i] = None;
}

bool QubicEngine::makeMove(int index, Player player) {
  if (index < 0 || index >= 64 || board[index] != None) return false;
  board[index] = player;
  return true;
}

QubicEngine::Player QubicEngine::checkWinner() const {
  for (const auto& line : winningLines) {
    uint8_t p1 = board[line.indices[0]];
    if (p1 != None && 
        p1 == board[line.indices[1]] && 
        p1 == board[line.indices[2]] && 
        p1 == board[line.indices[3]]) {
      return (Player)p1;
    }
  }
  return None;
}

bool QubicEngine::isFull() const {
  for (int i = 0; i < 64; i++) if (board[i] == None) return false;
  return true;
}

void QubicEngine::initializeLines() {
  winningLines.clear();

  // 1. Horizontal lines in each layer (4 * 4 = 16)
  for (int z = 0; z < 4; z++) {
    for (int y = 0; y < 4; y++) {
      Line l;
      for (int x = 0; x < 4; x++) l.indices[x] = z * 16 + y * 4 + x;
      winningLines.push_back(l);
    }
  }

  // 2. Vertical lines in each layer (4 * 4 = 16)
  for (int z = 0; z < 4; z++) {
    for (int x = 0; x < 4; x++) {
      Line l;
      for (int y = 0; y < 4; y++) l.indices[y] = z * 16 + y * 4 + x;
      winningLines.push_back(l);
    }
  }

  // 3. Pillar lines (across layers) (4 * 4 = 16)
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      Line l;
      for (int z = 0; z < 4; z++) l.indices[z] = z * 16 + y * 4 + x;
      winningLines.push_back(l);
    }
  }

  // 4. Diagonals in each flat layer (4 * 2 = 8)
  for (int z = 0; z < 4; z++) {
    Line d1, d2;
    for (int i = 0; i < 4; i++) {
        d1.indices[i] = z * 16 + i * 4 + i;
        d2.indices[i] = z * 16 + i * 4 + (3 - i);
    }
    winningLines.push_back(d1);
    winningLines.push_back(d2);
  }

  // 5. Diagonals in vertical X-Z planes (4 * 2 = 8)
  for (int y = 0; y < 4; y++) {
    Line d1, d2;
    for (int i = 0; i < 4; i++) {
      d1.indices[i] = i * 16 + y * 4 + i;
      d2.indices[i] = i * 16 + y * 4 + (3 - i);
    }
    winningLines.push_back(d1);
    winningLines.push_back(d2);
  }

  // 6. Diagonals in vertical Y-Z planes (4 * 2 = 8)
  for (int x = 0; x < 4; x++) {
    Line d1, d2;
    for (int i = 0; i < 4; i++) {
      d1.indices[i] = i * 16 + i * 4 + x;
      d2.indices[i] = i * 16 + (3 - i) * 4 + x;
    }
    winningLines.push_back(d1);
    winningLines.push_back(d2);
  }

  // 7. Space Diagonals (main diagonals of the cube) (4)
  Line sd1, sd2, sd3, sd4;
  for (int i = 0; i < 4; i++) {
    sd1.indices[i] = i * 16 + i * 4 + i;         // (0,0,0) to (3,3,3)
    sd2.indices[i] = i * 16 + i * 4 + (3 - i);   // (3,0,0) to (0,3,3)
    sd3.indices[i] = i * 16 + (3 - i) * 4 + i;   // (0,3,0) to (3,0,3)
    sd4.indices[i] = i * 16 + (3 - i) * 4 + (3 - i); // (3,3,0) to (0,0,3)
  }
  winningLines.push_back(sd1);
  winningLines.push_back(sd2);
  winningLines.push_back(sd3);
  winningLines.push_back(sd4);

  linesInitialized = true;
}

int QubicEngine::getBestMove(int difficultyLevel) {
  int bestScore = -1000000;
  int bestIndex = -1;
  std::vector<int> candidateMoves;
  
  // Determine how many lines to evaluate based on difficulty
  // L1: 40, L2: 55, L3: 70
  int linesToEval = 76;
  if (difficultyLevel == 1) linesToEval = 40;
  else if (difficultyLevel == 2) linesToEval = 55;
  else if (difficultyLevel == 3) linesToEval = 70;

  // Generate a random subset of winning lines for this turn
  std::vector<int> linesToEvalIndices(76);
  for (int i = 0; i < 76; i++) linesToEvalIndices[i] = i;

  if (linesToEval < 76) {
      // Fisher-Yates partial shuffle for the first `linesToEval` elements
      for (int i = 0; i < linesToEval; i++) {
          int swapIdx = i + random(76 - i);
          int temp = linesToEvalIndices[i];
          linesToEvalIndices[i] = linesToEvalIndices[swapIdx];
          linesToEvalIndices[swapIdx] = temp;
      }
  }

  for (int i = 0; i < 64; i++) {
    if (board[i] == None) {
      // Pass the fully prepared subset to evaluateMove
      int score = evaluateMove(i, AI, linesToEvalIndices, linesToEval);

      if (score > bestScore) {
        bestScore = score;
        bestIndex = i;
        candidateMoves.clear();
        candidateMoves.push_back(i);
      } else if (score == bestScore) {
        candidateMoves.push_back(i);
      }
    }
  }

  if (candidateMoves.empty()) return -1;
  // Randomize among equal scores for variety
  return candidateMoves[random(candidateMoves.size())];
}

int QubicEngine::evaluateMove(int index, Player player, const std::vector<int>& evalIndices, int linesToEval) {
  int totalScore = 0;
  Player opponent = (player == AI) ? Human : AI;

  for (int i = 0; i < linesToEval; i++) {
    const auto& line = winningLines[evalIndices[i]];
    bool onLine = false;
    for (int i = 0; i < 4; i++) if (line.indices[i] == index) { onLine = true; break; }
    if (!onLine) continue;

    int playerCount = 0;
    int opponentCount = 0;
    for (int i = 0; i < 4; i++) {
      uint8_t p = board[line.indices[i]];
      if (p == player) playerCount++;
      else if (p == opponent) opponentCount++;
    }

    // Heuristic Weights
    if (opponentCount == 0) {
      if (playerCount == 3) totalScore += 100000; // Win!
      else if (playerCount == 2) totalScore += 1000;  // Build potential
      else if (playerCount == 1) totalScore += 100;
      else totalScore += 10;
    } else if (playerCount == 0) {
      if (opponentCount == 3) totalScore += 50000;  // Block win
      else if (opponentCount == 2) totalScore += 500;
      else totalScore += 50;
    }
  }

  // Bonus for central positions
  int x = index % 4;
  int y = (index / 4) % 4;
  int z = index / 16;
  if ((x == 1 || x == 2) && (y == 1 || y == 2) && (z == 1 || z == 2)) totalScore += 5;

  return totalScore;
}
