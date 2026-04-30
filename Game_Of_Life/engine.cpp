#include "engine.h"

GameEngine engine;

GameEngine::GameEngine() {
    current_gen = nullptr;
    next_gen = nullptr;
}

void GameEngine::init() {
    current_gen = (uint8_t*)malloc(GRID_BUFFER_SIZE);
    next_gen = (uint8_t*)malloc(GRID_BUFFER_SIZE);
    memset(current_gen, 0, GRID_BUFFER_SIZE);
    memset(next_gen, 0, GRID_BUFFER_SIZE);
    
    randomSeed(analogRead(0));
    randomize();
}

void GameEngine::randomize() {
    for (int i = 0; i < GRID_BUFFER_SIZE; i++) {
        current_gen[i] = random(256);
    }
}

bool GameEngine::getCell(int x, int y) const {
    return getCellInternal(current_gen, x, y);
}

void GameEngine::setCell(int x, int y, bool alive) {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return;
    int idx = y * GRID_WIDTH_BYTES + (x >> 3);
    uint8_t mask = 0x80 >> (x & 7);
    if (alive) current_gen[idx] |= mask;
    else current_gen[idx] &= ~mask;
}

void GameEngine::computeNextGeneration() {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int alive_neighbors = 0;
            
            // 인접한 8개의 셀 상태 확인
            if (getCellInternal(current_gen, x - 1, y - 1)) alive_neighbors++;
            if (getCellInternal(current_gen, x,     y - 1)) alive_neighbors++;
            if (getCellInternal(current_gen, x + 1, y - 1)) alive_neighbors++;
            if (getCellInternal(current_gen, x - 1, y))     alive_neighbors++;
            if (getCellInternal(current_gen, x + 1, y))     alive_neighbors++;
            if (getCellInternal(current_gen, x - 1, y + 1)) alive_neighbors++;
            if (getCellInternal(current_gen, x,     y + 1)) alive_neighbors++;
            if (getCellInternal(current_gen, x + 1, y + 1)) alive_neighbors++;

            bool currently_alive = getCellInternal(current_gen, x, y);
            bool next_state = false;

            // Conway의 Game of Life 규칙
            if (currently_alive) {
                if (alive_neighbors == 2 || alive_neighbors == 3) next_state = true;
            } else {
                if (alive_neighbors == 3) next_state = true;
            }

            int idx = y * GRID_WIDTH_BYTES + (x >> 3);
            uint8_t mask = 0x80 >> (x & 7);
            if (next_state) next_gen[idx] |= mask;
            else next_gen[idx] &= ~mask;
        }
    }

    // 버퍼 스왑 (현재 세대 <- 다음 세대)
    uint8_t* temp = current_gen;
    current_gen = next_gen;
    next_gen = temp;
}
