#pragma once

#include <stdint.h>
#include <vector>

class QubicEngine {
 public:
  enum Player : uint8_t { None = 0, Human = 1, AI = 2 };

  struct Line {
    uint8_t indices[4];
  };

  QubicEngine();

  void reset();
  bool makeMove(int index, Player player);
  Player checkWinner() const;
  bool isFull() const;
  
  // AI Logic
  int getBestMove(int difficultyLevel = 3);
  
  uint8_t getAt(int index) const { return board[index]; }
  uint8_t getAt(int x, int y, int z) const { return board[z * 16 + y * 4 + x]; }

  static const std::vector<Line>& getWinningLines();

 private:
  uint8_t board[64]; // 4x4x4 board
  static std::vector<Line> winningLines;
  static bool linesInitialized;

  static void initializeLines();
  int evaluateMove(int index, Player player, const std::vector<int>& evalIndices, int linesToEval);
};
