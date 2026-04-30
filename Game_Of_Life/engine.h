#ifndef ENGINE_H
#define ENGINE_H

#include <Arduino.h>

#define GRID_WIDTH 512
#define GRID_HEIGHT 64
#define GRID_WIDTH_BYTES (GRID_WIDTH / 8)
#define GRID_BUFFER_SIZE (GRID_HEIGHT * GRID_WIDTH_BYTES)

class GameEngine {
public:
    GameEngine();
    void init();
    void computeNextGeneration();
    void randomize();
    bool getCell(int x, int y) const;
    void setCell(int x, int y, bool alive);
    
    // 포인터 반환 (디스플레이 렌더링용)
    const uint8_t* getCurrentBuffer() const { return current_gen; }

private:
    uint8_t* current_gen;
    uint8_t* next_gen;

    inline bool getCellInternal(const uint8_t* buf, int x, int y) const {
        // Toroidal wrap (가장자리가 반대편과 이어지도록 처리)
        if (x < 0) x = GRID_WIDTH - 1;
        else if (x >= GRID_WIDTH) x = 0;
        if (y < 0) y = GRID_HEIGHT - 1;
        else if (y >= GRID_HEIGHT) y = 0;
        
        // MSB-first 포맷
        return (buf[y * GRID_WIDTH_BYTES + (x >> 3)] & (0x80 >> (x & 7))) != 0;
    }
};

extern GameEngine engine;

#endif
