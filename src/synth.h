#pragma once
#include <cstdint>

void open_device(uint16_t num_channels, uint16_t sample_rate, uint16_t sample_bytes);
void close_device();
void play_sound(void* const buffer, uint32_t bytes);