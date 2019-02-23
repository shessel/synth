#include "synth.h"

int main()
{
    synth_init();
    synth_generate();
    synth_play();
    synth_deinit();

    return 0;
}
