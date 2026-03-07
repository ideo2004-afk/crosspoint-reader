#include "MiniGoEngine.h"
#include <algorithm>
#include <cmath>
#include <random>

MiniGoEngine::MiniGoEngine(int size) : size(size) {
    reset(size);
}

void MiniGoEngine::reset(int newSize) {
    if (newSize > 0) size = newSize;
    board.assign(size * size, EMPTY);
    lastBoard.assign(size * size, EMPTY);
    consecutivePasses = 0;
    mctsRoot.reset();
}

bool MiniGoEngine::isFirstMove() const {
    for (Color c : board) if (c != EMPTY) return false;
    return true;
}

MiniGoEngine::Color MiniGoEngine::getAt(int x, int y) const {
    if (x < 0 || x >= size || y < 0 || y >= size) return EMPTY;
    return board[y * size + x];
}

bool MiniGoEngine::wouldBeSuicide(int x, int y, Color player, const Color* currentBoard) const {
    for (int i = 0; i < size * size; i++) testBoard[i] = currentBoard[i];
    testBoard[y * size + x] = player;
    
    Color opponent = (player == BLACK ? WHITE : BLACK);
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    
    // Copy to temp board for checking captures
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < size && ny >= 0 && ny < size && testBoard[ny * size + nx] == opponent) {
            if (getGroup(nx, ny, testBoard).liberties == 0) return false; // This move is a capture, NOT suicide
        }
    }
    
    // Check if the new group has liberties
    return getGroup(x, y, testBoard).liberties == 0;
}

int MiniGoEngine::captureStones(int x, int y, Color opponent, Color* targetBoard) const {
    int totalCaptured = 0;
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < size && ny >= 0 && ny < size && targetBoard[ny * size + nx] == opponent) {
            Group g = getGroup(nx, ny, targetBoard);
            if (g.liberties == 0) {
                for (int j = 0; j < g.stoneCount; j++) {
                    targetBoard[g.stones[j]] = EMPTY;
                }
                totalCaptured += g.stoneCount;
            }
        }
    }
    return totalCaptured;
}

bool MiniGoEngine::isValidMove(Move move, Color player) const {
    if (move.pass || move.resign) return true;
    if (getAt(move.x, move.y) != EMPTY) return false;

    // Rule: First move cannot be in the center
    if (isFirstMove() && move.x == size / 2 && move.y == size / 2) return false;

    if (wouldBeSuicide(move.x, move.y, player, board.data())) return false;

    // Simple Ko check
    for (int i = 0; i < size * size; i++) testBoard[i] = board[i];
    testBoard[move.y * size + move.x] = player;
    captureStones(move.x, move.y, (player == BLACK ? WHITE : BLACK), testBoard);
    
    bool isKo = true;
    for (int i = 0; i < size * size; i++) {
        if (testBoard[i] != lastBoard[i]) {
            isKo = false;
            break;
        }
    }
    if (isKo) return false;

    return true;
}

bool MiniGoEngine::makeMove(Move move, Color player) {
    if (!isValidMove(move, player)) return false;

    lastBoard = board;
    
    if (move.pass) {
        consecutivePasses++;
    } else if (move.resign) {
        consecutivePasses = 2; // Treat as game over
    } else {
        consecutivePasses = 0;
        board[move.y * size + move.x] = player;

        // Execute captures
        Color opponent = (player == BLACK) ? WHITE : BLACK;
        const int dx[] = {0, 0, 1, -1};
        const int dy[] = {1, -1, 0, 0};
        captureStones(move.x, move.y, opponent, board.data());
    }
    return true;
}

MiniGoEngine::Group MiniGoEngine::getGroup(int x, int y, const Color* currentBoard) const {
    Color color = currentBoard[y * size + x];
    Group group;
    group.stoneCount = 0;
    group.liberties = 0;

    for (int i = 0; i < size * size; i++) visitedBuf[i] = false;

    int head = 0;
    int tail = 0;
    stackBuf[tail++] = y * size + x;
    visitedBuf[y * size + x] = true;

    while (head < tail) {
        int pos = stackBuf[head++];
        if (group.stoneCount < 81) {
            group.stones[group.stoneCount++] = pos;
        }
        int cx = pos % size;
        int cy = pos / size;

        const int dx[] = {0, 0, 1, -1};
        const int dy[] = {1, -1, 0, 0};
        for (int i = 0; i < 4; i++) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
                int npos = ny * size + nx;
                if (currentBoard[npos] == EMPTY) {
                    if (!visitedBuf[npos]) {
                        visitedBuf[npos] = true;
                        group.liberties++;
                    }
                } else if (currentBoard[npos] == color && !visitedBuf[npos]) {
                    visitedBuf[npos] = true;
                    stackBuf[tail++] = npos;
                }
            }
        }
    }
    return group;
}

float MiniGoEngine::calculateScore(Color player) const {
    float score = 0;
    for (int i = 0; i < size * size; i++) visitedBuf[i] = false;

    for (int i = 0; i < size * size; i++) {
        if (board[i] == player) {
            score += 1.0f;
        } else if (board[i] == EMPTY && !visitedBuf[i]) {
            // Check territory using pre-allocated buffers
            int regionCount = 0;
            int head = 0, tail = 0;
            stackBuf[tail++] = i;
            visitedBuf[i] = true;
            bool touchesBlack = false;
            bool touchesWhite = false;

            while(head < tail){
                int pos = stackBuf[head++];
                regionCount++;
                int cx = pos % size;
                int cy = pos / size;

                const int dx[] = {0, 0, 1, -1};
                const int dy[] = {1, -1, 0, 0};
                for(int j=0; j<4; j++){
                    int nx = cx + dx[j];
                    int ny = cy + dy[j];
                    if(nx >= 0 && nx < size && ny >= 0 && ny < size){
                        int npos = ny * size + nx;
                        if(board[npos] == BLACK) touchesBlack = true;
                        else if(board[npos] == WHITE) touchesWhite = true;
                        else if(!visitedBuf[npos]){
                            visitedBuf[npos] = true;
                            if (tail < 81) stackBuf[tail++] = npos;
                        }
                    }
                }
            }

            if (touchesBlack && !touchesWhite && player == BLACK) score += regionCount;
            else if (touchesWhite && !touchesBlack && player == WHITE) score += regionCount;
        }
    }
    // Komi
    if (player == WHITE) score += 0.5f; 
    return score;
}

MiniGoEngine::Color MiniGoEngine::getWinner() const {
    float black = calculateScore(BLACK);
    float white = calculateScore(WHITE);
    return (black > white) ? BLACK : WHITE;
}

// Simple MCTS implementation
void MiniGoEngine::startMCTS(Color aiColor) {
    mctsAiColor = aiColor;
    Color opponent = (aiColor == BLACK) ? WHITE : BLACK;
    mctsRoot = std::make_unique<Node>(Move::Pass(), nullptr, aiColor);
    
    // Expand root with all valid moves and add central bias
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            Move m = Move::Play(x, y);
            if (isValidMove(m, aiColor)) {
                auto child = std::make_unique<Node>(m, mctsRoot.get(), opponent);
                // Heuristic Bias: Give central 3x3 area a head start
                int mid = size / 2;
                if (abs(x - mid) <= 1 && abs(y - mid) <= 1) {
                    child->visits = 5;
                    child->wins = 2.5f; // Neutral winrate to start
                    mctsRoot->visits += 5;
                }
                mctsRoot->children.push_back(std::move(child));
            }
        }
    }
    mctsRoot->children.push_back(std::make_unique<Node>(Move::Pass(), mctsRoot.get(), opponent));
}

void MiniGoEngine::runMCTSSteps(int count) {
    if (!mctsRoot) return;
    for (int i = 0; i < count; i++) {
        Node* node = mctsRoot.get();
        std::vector<Color> simBoard = board;
        int simConsecutivePasses = consecutivePasses;
        
        // Selection
        while (!node->children.empty()) {
            node = selectBestChild(node, 1.414f);
            if (node->move.pass) {
                simConsecutivePasses++;
            } else if (simBoard[node->move.y * size + node->move.x] == EMPTY) {
                simBoard[node->move.y * size + node->move.x] = (node->playerToMove == BLACK) ? WHITE : BLACK;
            }
            if (simConsecutivePasses >= 2) break; 
        }

        // Simulation
        // Make a copy for simulation rollout
        Color rolloutBoard[81];
        for (int j = 0; j < size * size; j++) rolloutBoard[j] = simBoard[j];
        float result = simulate(rolloutBoard, node->playerToMove, mctsAiColor);
        
        // Backpropagation
        backpropagate(node, result);

        // Early Stopping check every 10 steps, minimum 100 sims
        if (i > 99 && i % 10 == 0 && shouldStopEarly()) break;
    }
}

bool MiniGoEngine::shouldStopEarly() const {
    if (!mctsRoot || mctsRoot->visits < 100) return false;
    Node* best = nullptr;
    Node* secondBest = nullptr;
    for (auto& child : mctsRoot->children) {
        if (!best || child->visits > best->visits) {
            secondBest = best;
            best = child.get();
        } else if (!secondBest || child->visits > secondBest->visits) {
            secondBest = child.get();
        }
    }
    if (best && secondBest) {
        // If best move has more than 2x visits than second best, it's very likely the winner
        if (best->visits > secondBest->visits * 2) return true;
    } else if (best && mctsRoot->children.size() == 1) {
        return true; // Only one move possible
    }
    return false;
}

MiniGoEngine::Move MiniGoEngine::finishMCTS() {
    if (!mctsRoot) return Move::Pass();
    Node* best = selectBestChild(mctsRoot.get(), 0); 
    Move m = best ? best->move : Move::Pass();
    mctsRoot.reset();
    return m;
}

MiniGoEngine::Move MiniGoEngine::getBestMove(Color aiColor, int simulations) {
    startMCTS(aiColor);
    runMCTSSteps(simulations);
    return finishMCTS();
}

MiniGoEngine::Node* MiniGoEngine::selectBestChild(Node* node, float exploration) {
    Node* best = nullptr;
    float bestScore = -1e9;
    
    for (auto& child : node->children) {
        float score;
        if (exploration == 0) {
            score = (child->visits == 0) ? 0 : child->wins / child->visits;
        } else {
            if (child->visits == 0) score = 1e6;
            else score = (child->wins / child->visits) + exploration * sqrt(log(node->visits) / child->visits);
        }
        
        if (score > bestScore) {
            bestScore = score;
            best = child.get();
        }
    }
    return best;
}

bool MiniGoEngine::isEye(int x, int y, Color color, const Color* currentBoard) const {
    if (currentBoard[y * size + x] != EMPTY) return false;
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
            if (currentBoard[ny * size + nx] != color) return false;
        }
    }
    return true;
}

int MiniGoEngine::countLiberties(int x, int y, const Color* currentBoard) const {
    if (currentBoard[y * size + x] == EMPTY) return 0;
    return getGroup(x, y, currentBoard).liberties;
}

float MiniGoEngine::calculateInfluence(const Color* currentBoard) const {
    int influence[81] = {0};
    for (int i = 0; i < size * size; i++) {
        if (currentBoard[i] == EMPTY) continue;
        int x = i % size, y = i / size;
        int val = (currentBoard[i] == BLACK) ? 3 : -3;
        influence[i] += val;
        
        const int dx[] = {0, 0, 1, -1};
        const int dy[] = {1, -1, 0, 0};
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
                influence[ny * size + nx] += (val > 0 ? 1 : -1);
            }
        }
    }
    
    float blackScore = 0;
    float whiteScore = 0.5f; // Komi
    for (int i = 0; i < size * size; i++) {
        if (influence[i] > 0) blackScore += 1.0f;
        else if (influence[i] < 0) whiteScore += 1.0f;
    }
    return (blackScore - whiteScore);
}

float MiniGoEngine::simulate(Color* simBoard, Color toMove, Color aiColor) {
    // Strategic Mid-game Evaluation:
    // Instead of playing till the end, simulate fewer steps and judge by influence.
    int maxDepth = (size == 5) ? 15 : 30; 
    int consecutivePassesRollout = 0;
    int depth = 0;

    int candidates[81];
    int captures[81];

    while (depth < maxDepth && consecutivePassesRollout < 2) {
        int candCount = 0;
        int capCount = 0;
        Color opponent = (toMove == BLACK) ? WHITE : BLACK;
        
        for (int i = 0; i < size * size; i++) {
            if (simBoard[i] == EMPTY) {
                int x = i % size, y = i / size;
                if (isEye(x, y, toMove, simBoard)) continue;
                
                bool canCapture = false;
                const int dx[] = {0,0,1,-1}, dy[] = {1,-1,0,0};
                for(int k=0; k<4; k++) {
                    int nx=x+dx[k], ny=y+dy[k];
                    if(nx>=0 && nx<size && ny>=0 && ny<size && simBoard[ny*size+nx] == opponent) {
                        if (getGroup(nx, ny, simBoard).liberties == 1) { 
                            canCapture = true;
                            break;
                        }
                    }
                }

                if (canCapture) {
                    captures[capCount++] = i;
                } else if (!wouldBeSuicide(x, y, toMove, simBoard)) {
                    candidates[candCount++] = i;
                }
            }
        }

        int movePos = -1;
        if (capCount > 0) movePos = captures[rand() % capCount];
        else if (candCount > 0) movePos = candidates[rand() % candCount];

        if (movePos != -1) {
            int x = movePos % size, y = movePos / size;
            simBoard[movePos] = toMove;
            captureStones(x, y, opponent, simBoard);
            consecutivePassesRollout = 0;
        } else {
            consecutivePassesRollout++;
        }
        
        toMove = opponent;
        depth++;
    }

    float diff = calculateInfluence(simBoard);
    if (aiColor == BLACK) return (diff > 0) ? 1.0f : 0.0f;
    return (diff < 0) ? 1.0f : 0.0f;
}

void MiniGoEngine::backpropagate(Node* node, float result) {
    while (node) {
        node->visits++;
        node->wins += result;
        node = node->parent;
    }
}
