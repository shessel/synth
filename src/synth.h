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
void synth_generate(ADSR adsr);
void synth_play();