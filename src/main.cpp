#include "synth.h"

int main()
{
    synth_init();
    track track = {
        {
            {60.0f, 1u},
            {14000, 3u},
            {3000.0, 2u},
            {14000, 3u},
    
            {60.0f, 1u},
            {14000, 3u},
            {3500.0, 2u},
            {14000, 3u},
    
            {60.0f, 1u},
            {60.0f, 1u},
            {2400.0f, 2u},
            {60.0f, 1u},
    
            {440.0f, 4u},
            {554.46f, 4u},
            {659.25f, 4u},
            {554.46f, 4u},
        }
    };
    int16_t* track_buffer = synth_generate_track(track);
   // synth_queue_track(track_buffer, 2);

    sound_desc desc0 = {};
    desc0.amplitude = 0.5f;
    desc0.frequency = 440.0f;
    desc0.amplitude_modifier_id = 3;
    desc0.amplitude_modifier_params = { 0.0f, 1.0f };
    desc0.base_sound_id = 0;

    sound_desc desc1 = {};
    desc1.amplitude = 0.5f;
    desc1.frequency = 880.0f / 3.0f;
    desc1.frequency_modifier_id = 3;
    desc1.frequency_modifier_params = { 0.25f, 0.75f };
    desc1.base_sound_id = 0;
    
    sound_desc descs[] = {
        desc0,
        desc1,
    };

    int16_t* sound_buffer = synth_generate_sound(descs, sizeof(descs)/sizeof(descs[0]));
    synth_queue_sound(sound_buffer, 4);

    synth_deinit();
    delete[] track_buffer;
    delete[] sound_buffer;

    return 0;
}
