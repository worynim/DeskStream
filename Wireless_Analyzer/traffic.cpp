// worynim@gmail.com
/**
 * @file traffic.cpp
 * @brief traffic.h 구현
 */
#include "traffic.h"

namespace wa {

void trafficUpdate(ChannelTraffic& t, uint32_t frames, uint32_t dwellUs, float alpha) {
    if (dwellUs == 0) return;
    const float fps = frames * 1e6f / dwellUs;
    if (t.sweeps == 0) t.fps = fps;
    else t.fps += alpha * (fps - t.fps);
    if (t.sweeps < 0xFFFF) t.sweeps++;
}

}  // namespace wa
