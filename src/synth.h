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

struct modifier_params
{
    float begin;
    float end;
};

struct sound_desc
{
    uint8_t base_sound_id;
    float amplitude;
    float frequency;
    uint8_t frequency_modifier_id;
    uint8_t amplitude_modifier_id;
    modifier_params frequency_modifier_params;
    modifier_params amplitude_modifier_params;
};


void synth_init();
void synth_deinit();
void synth_generate(ADSR adsr = { 0.01f, 0.01f, 1.0f, 0.95f });
int16_t* synth_generate_track(const track& track);
int16_t* synth_generate_sound(const sound_desc* sound_desc, uint8_t num_sound_descs);
void synth_queue_track(int16_t* track_buffer, uint32_t loop_count = 1);
void synth_queue_sound(int16_t* sound_buffer, uint32_t loop_count = 1);
void synth_play(int16_t* buffer = nullptr, uint32_t buffer_sizeu = 0, uint32_t loop_count = 1);