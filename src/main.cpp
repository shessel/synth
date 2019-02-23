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
    synth_queue_track(track_buffer, 2);

    sound_desc desc0;
    desc0.amplitude = 1.0f;
    desc0.amplitude_modifier_id = 2;
    desc0.amplitude_modifier_params = { 0.0f, 1.0f };
    desc0.base_sound_id = 0;
    
    sound_desc desc[] = {
        desc0,
    };

    int16_t* sound_buffer = synth_generate_sound(desc, 1);
    synth_queue_sound(sound_buffer, 12);

    synth_deinit();
    delete[] track_buffer;
    delete[] sound_buffer;

    return 0;
}
