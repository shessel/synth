#pragma once

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
void synth_play();