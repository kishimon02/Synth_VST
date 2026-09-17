#pragma once

#include <cstdint>

namespace wf
{

// White (xorshift) or pink (Paul Kellet's economy filter) noise, -1..1.
class NoiseOscillator
{
public:
    enum Type { white = 0, pink };

    void seed (uint32_t s) noexcept { state = s != 0 ? s : 0x9E3779B9u; }
    void setType (int t) noexcept { type = t; }

    inline float process() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        const float w = (float) (state & 0xFFFFFF) * (2.0f / 16777216.0f) - 1.0f;
        if (type == white)
            return w;

        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        return (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
    }

private:
    uint32_t state = 0x12345678u;
    int type = white;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
};

} // namespace wf
