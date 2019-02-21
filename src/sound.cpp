#include <Windows.h>
#include <cstdint>
#include <atomic>
#include <iostream>

#include "sound.h"

static HWAVEOUT hwo = 0;

static constexpr uint8_t NUM_HEADERS = 8;
WAVEHDR headers[NUM_HEADERS];
std::atomic_int_fast8_t free_header_count = NUM_HEADERS;
std::atomic_int_fast8_t current_header = 0;

void CALLBACK waveOutProc(
    HWAVEOUT  hwoo,
    UINT      uMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
) {
    switch (uMsg)
    {
    case WOM_CLOSE:
        std::cout << "[callback] device closed" << std::endl;
        break;
    case WOM_OPEN:
        std::cout << "[callback] device opened" << std::endl;
        break;
    case WOM_DONE:
    {
        std::cout << "[callback] header done" << std::endl;
        ++free_header_count;
    }
    uMsg *= 1;
    break;
    default:
        uMsg *= 1;
        uMsg;
    }
    hwoo;
    uMsg;
    dwInstance;
    dwParam1;
    dwParam2;
};

void sound_open_device(uint16_t sample_rate, uint16_t sample_bytes, uint16_t num_channels)
{
    WAVEFORMATEX fmt{};
    fmt.wBitsPerSample = sample_bytes * 8;
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = num_channels;
    fmt.nSamplesPerSec = sample_rate;
    fmt.nAvgBytesPerSec = sample_rate * num_channels * sample_bytes;
    fmt.nBlockAlign = num_channels * sample_bytes;

    waveOutOpen(&hwo, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(&waveOutProc), 0, CALLBACK_FUNCTION);
}

void sound_close_device()
{
    while (free_header_count < NUM_HEADERS)
    {
        Sleep(10);
    }
    for (size_t i = 0; i < NUM_HEADERS; ++i)
    {
        if (headers[i].dwFlags & WHDR_PREPARED)
        {
            std::cout << "[close] unpreparing header " << i << std::endl;
            waveOutUnprepareHeader(hwo, &headers[i], sizeof(WAVEHDR));
        }
    }
    waveOutClose(hwo);
}

void sound_queue_buffer(void* const buffer, uint32_t bytes)
{
    if (--free_header_count >= 0)
    {
        auto hdr_id = (++current_header) % NUM_HEADERS;
        WAVEHDR& hdr = headers[hdr_id];
        if (hdr.dwFlags & WHDR_PREPARED)
        {
            std::cout << "[play] unpreparing header" << std::endl;
            waveOutUnprepareHeader(hwo, &hdr, sizeof(WAVEHDR));
        }
        //hdr.dwLoops = static_cast<DWORD>(-1);
        hdr.lpData = reinterpret_cast<LPSTR>(buffer);
        hdr.dwBufferLength = bytes;
        //hdr.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
        waveOutPrepareHeader(hwo, &hdr, sizeof(WAVEHDR));
        waveOutWrite(hwo, &hdr, sizeof(WAVEHDR));
        //Sleep(static_cast<DWORD>(2000 + 50));
        waveOutUnprepareHeader(hwo, &hdr, sizeof(WAVEHDR));
    }
    else
    {
        std::cout << "[play] no free header" << std::endl;
        ++free_header_count;
        return;
    }
}