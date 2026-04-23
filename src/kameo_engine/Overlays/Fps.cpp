#include "Fps.h"

void FPSCounter::Tick() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> delta = now - lastTick;
    lastTick = now;

    float ms = static_cast<float>(delta.count());
    frameTimes.push_back(ms);
    if (static_cast<int>(frameTimes.size()) > AverageCount) {
        frameTimes.erase(frameTimes.begin());
    }

    float total = 0.0f;
    for (float f : frameTimes) total += f;
    averageMs  = total / static_cast<float>(frameTimes.size());
    averageFps = averageMs > 0.0f ? 1000.0f / averageMs : 0.0f;
}
