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
    Color localTestBoard[81];
    for (int i = 0; i < size * size; i++) localTestBoard[i] = currentBoard[i];
    localTestBoard[y * size + x] = player;
    
    Color opponent = (player == BLACK ? WHITE : BLACK);
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    
    // Copy to temp board for checking captures
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < size && ny >= 0 && ny < size && localTestBoard[ny * size + nx] == opponent) {
            if (getGroup(nx, ny, localTestBoard).liberties == 0) return false; // This move is a capture, NOT suicide
        }
    }
    
    // Check if the new group has liberties
    return getGroup(x, y, localTestBoard).liberties == 0;
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
    Color localTestBoard[81];
    for (int i = 0; i < size * size; i++) localTestBoard[i] = board[i];
    localTestBoard[move.y * size + move.x] = player;
    captureStones(move.x, move.y, (player == BLACK ? WHITE : BLACK), localTestBoard);
    
    bool isKo = true;
    for (int i = 0; i < size * size; i++) {
        if (localTestBoard[i] != lastBoard[i]) {
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

    bool localVisited[81] = {false};
    int localStack[81];

    int head = 0;
    int tail = 0;
    localStack[tail++] = y * size + x;
    localVisited[y * size + x] = true;

    while (head < tail) {
        int pos = localStack[head++];
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
                    if (!localVisited[npos]) {
                        localVisited[npos] = true;
                        group.liberties++;
                    }
                } else if (currentBoard[npos] == color && !localVisited[npos]) {
                    localVisited[npos] = true;
                    localStack[tail++] = npos;
                }
            }
        }
    }
    return group;
}

float MiniGoEngine::calculateScore(Color player) const {
    float score = 0;
    bool localVisited[81] = {false};
    int localStack[81];

    for (int i = 0; i < size * size; i++) {
        if (board[i] == player) {
            score += 1.0f;
        } else if (board[i] == EMPTY && !localVisited[i]) {
            // Check territory using pre-allocated buffers
            int regionCount = 0;
            int head = 0, tail = 0;
            localStack[tail++] = i;
            localVisited[i] = true;
            bool touchesBlack = false;
            bool touchesWhite = false;

            while(head < tail){
                int pos = localStack[head++];
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
                        else if(!localVisited[npos]){
                            localVisited[npos] = true;
                            if (tail < 81) localStack[tail++] = npos;
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
    
    // Shuffle children to prevent deterministic ties always favoring the top-left (first generated)
    int n = mctsRoot->children.size();
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        std::swap(mctsRoot->children[i], mctsRoot->children[j]);
    }
}

void MiniGoEngine::runMCTSSteps(int count) {
    if (!mctsRoot) return;
    for (int i = 0; i < count; i++) {
        Node* node = mctsRoot.get();
        Color simBoard[81];
        for (int j = 0; j < size * size; j++) simBoard[j] = board[j];
        int simConsecutivePasses = consecutivePasses;
        
        // Selection
        while (!node->children.empty()) {
            node = selectBestChild(node, 1.414f);
            if (node->move.pass) {
                simConsecutivePasses++;
            } else if (simBoard[node->move.y * size + node->move.x] == EMPTY) {
                simBoard[node->move.y * size + node->move.x] = (node->playerToMove == BLACK) ? WHITE : BLACK;
                captureStones(node->move.x, node->move.y, node->playerToMove, simBoard); // Correctly clean up captured stones for simulation state
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
        float winRate1 = best->wins / best->visits;
        float winRate2 = secondBest->wins / secondBest->visits;
        // If best is significantly better (>20% win gap) after 60+ sims
        if (winRate1 > winRate2 + 0.20f) return true;
    } else if (best && mctsRoot->children.size() == 1) {
        return true; // Only one move possible
    }
    return false;
}

float MiniGoEngine::calculateInfluence() const {
    return calculateInfluence(board.data());
}

MiniGoEngine::Move MiniGoEngine::finishMCTS() {
    if (!mctsRoot) return Move::Pass();
    Node* best = selectBestChild(mctsRoot.get(), 0); 
    
    // Flaw 3: Auto-Pass / Resign Awareness
    // If the best move we found has a dismal win rate (< 5%), just pass.
    if (best && best->visits > 0) {
        float winRate = best->wins / best->visits;
        if (winRate < 0.05f) {
            mctsRoot.reset();
            return Move::Pass();
        }
    }
    
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
    Node* bestChild = nullptr;
    float bestScore = -1e9;
    
    float logVisits = log(node->visits > 0 ? node->visits : 1);

    for (auto& child : node->children) {
        float score;
        if (exploration == 0) {
            score = child->visits; // Base the final selection purely on visit count, NOT win rate.
        } else if (child->visits == 0) {
            score = 10000.0f; // Strongly favor unvisited nodes!
        } else {
            float exploitation = child->wins / child->visits;
            float exploration_term = exploration * sqrt(logVisits / child->visits);
            score = exploitation + exploration_term;
        }

        // Apply heuristics ONLY at the root node (when selecting the actual move to play)
        // AND apply it even when exploration == 0 to act as a tie-breaker for nodes with identical visit counts!
        if (node == mctsRoot.get()) {
            // Connectivity & Liberty Bias (Flaw 1: 連氣)
            float connectivityBias = 0.0f;
            int x = child->move.x;
            int y = child->move.y;
            bool connectsToFriendly = false;
            
            const int dx[] = {0, 0, 1, -1};
            const int dy[] = {1, -1, 0, 0};
            for (int k = 0; k < 4; k++) {
                int nx = x + dx[k];
                int ny = y + dy[k];
                if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
                    if (board[ny * size + nx] == node->playerToMove) { // Connects to own stone
                        connectsToFriendly = true;
                        connectivityBias += 0.1f;
                    } else if (board[ny * size + nx] != EMPTY) { // Touches enemy
                        connectivityBias += 0.05f; // Good to stay close for combat
                    }
                }
            }
            
            // If it connects to a friendly stone, strongly prefer it if the resulting group is relatively safe (>= 3 liberties)
            // This simulates "extending liberties". A full check would be too slow, so we approximate or use the bias.
            if (connectsToFriendly) {
                 connectivityBias += 0.15f; 
            }

            // Central bias (slight) -> Increased to encourage center play
            float distCenter = sqrt(pow(x - size/2.0f, 2) + pow(y - size/2.0f, 2));
            float centralBias = (size - distCenter) * 0.08f;
            
            // When exploration == 0 (Final Selection), scale heuristics up so they can break ties in integer visit counts.
            if (exploration == 0) {
                score += (connectivityBias + centralBias) * 5.0f; 
            } else {
                score += connectivityBias + centralBias;
            }
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestChild = child.get();
        }
    }
    return bestChild;
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
    int groupLibertiesCache[81];
    for(int i=0; i<81; i++) groupLibertiesCache[i] = -1;

    for (int i = 0; i < size * size; i++) {
        if (currentBoard[i] == EMPTY) continue;
        
        int liberties = groupLibertiesCache[i];
        if (liberties == -1) {
            int x = i % size, y = i / size;
            auto group = getGroup(x, y, currentBoard);
            liberties = group.liberties;
            for (int k = 0; k < group.stoneCount; k++) {
                groupLibertiesCache[group.stones[k]] = liberties;
            }
        }
        
        // Liberty-weighted influence: a stone with more liberties is "stronger"
        int weight = (liberties >= 3) ? 4 : (liberties == 2 ? 3 : 1);
        int val = (currentBoard[i] == BLACK) ? weight : -weight;
        
        influence[i] += val;
        
        int x = i % size, y = i / size;
        const int dx[] = {0, 0, 1, -1};
        const int dy[] = {1, -1, 0, 0};
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
                // Neighbors get less influence
                influence[ny * size + nx] += (val > 0 ? 1 : -1);
            }
        }
    }
    
    float blackScore = 0;
    float whiteScore = 0.5f; // Komi
    for (int i = 0; i < size * size; i++) {
        if (influence[i] > 1) blackScore += 1.0f;
        else if (influence[i] < -1) whiteScore += 1.0f;
    }
    return (blackScore - whiteScore);
}

float MiniGoEngine::simulate(Color* simBoard, Color toMove, Color aiColor) {
    int maxDepth = 8; // "Smart Fast-Chess": 4 moves per side max per rollout
    int consecutivePassesRollout = 0;
    int depth = 0;
    int lastMovePos = -1;

    while (depth < maxDepth && consecutivePassesRollout < 2) {
        Color opponent = (toMove == BLACK) ? WHITE : BLACK;
        int movePos = -1;

        // 1. Local Tactical Heuristics (O(1) instead of O(N^3))
        if (lastMovePos != -1) {
            int lx = lastMovePos % size, ly = lastMovePos / size;
            
            // A. Did the opponent just put themselves in Atari?
            auto g = getGroup(lx, ly, simBoard);
            if (g.liberties == 1) {
                 for (int k = 0; k < g.stoneCount && movePos == -1; k++) {
                     int sx = g.stones[k] % size, sy = g.stones[k] / size;
                     const int dx[] = {0,0,1,-1}, dy[] = {1,-1,0,0};
                     for(int d=0; d<4; d++) {
                         int nx = sx + dx[d], ny = sy + dy[d];
                         if (nx>=0 && nx<size && ny>=0 && ny<size && simBoard[ny*size+nx] == EMPTY) {
                             if (!wouldBeSuicide(nx, ny, toMove, simBoard)) {
                                 movePos = ny * size + nx;
                                 break;
                             }
                         }
                     }
                 }
            }

            // B. Did the opponent's move put OUR group in Atari? (70% chance to respond)
            if (movePos == -1 && rand() % 10 < 7) {
                 const int dx[] = {0,0,1,-1}, dy[] = {1,-1,0,0};
                 for(int d=0; d<4; d++) {
                     int nx = lx + dx[d], ny = ly + dy[d];
                     if (nx>=0 && nx<size && ny>=0 && ny<size && simBoard[ny*size+nx] == toMove) {
                         auto myGroup = getGroup(nx, ny, simBoard);
                         if (myGroup.liberties == 1) {
                             for (int k = 0; k < myGroup.stoneCount && movePos == -1; k++) {
                                 int sx = myGroup.stones[k] % size, sy = myGroup.stones[k] / size;
                                 for(int d2=0; d2<4; d2++) {
                                     int nnx = sx + dx[d2], nny = sy + dy[d2];
                                     if (nnx>=0 && nnx<size && nny>=0 && nny<size && simBoard[nny*size+nnx] == EMPTY) {
                                         if (!wouldBeSuicide(nnx, nny, toMove, simBoard)) {
                                             movePos = nny * size + nnx;
                                             break;
                                         }
                                     }
                                 }
                             }
                         }
                         if (movePos != -1) break;
                     }
                 }
            }
        }

        // 2. Fast Random Selection
        if (movePos == -1) {
            int empties[81];
            int emptyCount = 0;
            for (int i = 0; i < size * size; i++) {
                if (simBoard[i] == EMPTY && !isEye(i%size, i/size, toMove, simBoard)) {
                    empties[emptyCount++] = i;
                }
            }

            if (emptyCount > 0) {
                // Fisher-Yates shuffle (only if more than 1 item)
                if (emptyCount > 1) {
                    for (int i = emptyCount - 1; i > 0; i--) {
                        int j = rand() % (i + 1);
                        int temp = empties[i]; empties[i] = empties[j]; empties[j] = temp;
                    }
                }

                // Just pick the first safe empty space from the shuffled list
                for (int i = 0; i < emptyCount; i++) {
                    int x = empties[i] % size, y = empties[i] / size;
                    if (!wouldBeSuicide(x, y, toMove, simBoard)) {
                        movePos = empties[i];
                        break;
                    }
                }
            }
        }

        if (movePos != -1) {
            int x = movePos % size, y = movePos / size;
            simBoard[movePos] = toMove;
            captureStones(x, y, opponent, simBoard);
            consecutivePassesRollout = 0;
            lastMovePos = movePos;
        } else {
            consecutivePassesRollout++;
            lastMovePos = -1;
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
