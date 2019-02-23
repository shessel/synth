#pragma once
#include <cstdint>

struct row
{
    float note;
    uint8_t sample_id;
};

constexpr size_t ROWS_PER_TRACK = 16;
struct track
{
    row rows[ROWS_PER_TRACK];
};

struct ADSR
{
    float a;
    float d;
    float s;
    float r;
};

void synth_init();
void synth_deinit();
void synth_generate(ADSR adsr = { 0.01f, 0.01f, 1.0f, 0.95f });
void synth_generate(const track& track);
void synth_play();