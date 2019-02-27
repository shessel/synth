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
int16_t* synth_generate_track(const track& track);
void synth_queue_track(int16_t* track_buffer, uint32_t loop_count);
void synth_play(int16_t* buffer = nullptr, uint32_t buffer_sizeu = 0, uint32_t loop_count = 1);