#pragma once

#include <cstdint>

void sound_open_device(uint16_t sample_rate, uint16_t sample_bytes, uint16_t num_channels);
void sound_close_device();
void sound_queue_buffer(void* const buffer, uint32_t bytes, uint32_t loop_count = 1);