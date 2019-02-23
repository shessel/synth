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
    synth_deinit();
    delete[] track_buffer;

    return 0;
}
