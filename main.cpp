#include <cstdint>
#include <cmath>

#include <Windows.h>

#include <iostream>
#include <vector>

static constexpr float PI = 3.14159265f;

static constexpr uint32_t SAMPLE_RATE = 44100;

static constexpr size_t BPM = 120;
static constexpr size_t MEASURES = 1;
static constexpr size_t METER_IN_4TH = 1;
static constexpr float BEAT_DURATION_SEC = 60.0 / BPM;
static constexpr size_t BEATS_TOTAL = MEASURES * METER_IN_4TH;
static constexpr float DURATION_SEC = BEATS_TOTAL * BEAT_DURATION_SEC;
static constexpr size_t NUM_CHANNELS = 2;
static constexpr size_t BYTE_PER_SAMPLE = 2;
static constexpr size_t BITS_PER_SAMPLE = BYTE_PER_SAMPLE * 8;
static constexpr size_t SAMPLE_COUNT = static_cast<size_t>(SAMPLE_RATE * NUM_CHANNELS * DURATION_SEC);
static constexpr size_t SAMPLE_OFFSET = 44 / BYTE_PER_SAMPLE; // WAV_HEADER is 44 bytes, so 22 uint16 elements
static constexpr size_t BUFFER_COUNT = SAMPLE_OFFSET + SAMPLE_COUNT;

// platform is little-endian, so least significant byte comes first in memory
// => bytes are reversed when specifying multiple bytes as one int
static constexpr uint32_t WAV_HEADER[] = {
    // Main chunk
    0x46464952, // ChunkId = "RIFF"
    BUFFER_COUNT * BYTE_PER_SAMPLE - 8,// ChunkSize, size of the main chunk, excluding the first two 4-byte fields
    0x45564157, // Format = "WAVE"
    // WAVE format has 2 sub chunks, fmt and data

    // Sub chunk fmt
    0x20746d66, // SubChunkId = "fmt "
    16, // SubChunkSize, size of sub chunk, excluding the first two 4-byte fields
    NUM_CHANNELS << 16 | 1, // 2 bytes AudioFormat = 1 for PCM, 2 bytes NumChannels,
    SAMPLE_RATE, // SampleRate
    SAMPLE_RATE * NUM_CHANNELS * BYTE_PER_SAMPLE, // ByteRate = SampleRate * NumChannels * BytesPerSample
    BITS_PER_SAMPLE << 16 | NUM_CHANNELS * BYTE_PER_SAMPLE, // 2 bytes BlockAlign = NumChannels * BytesPerSample, 2 bytes BitsPerSample

    // Sub chunk data
    0x61746164, // SubChunkId = "data"
    SAMPLE_COUNT * BYTE_PER_SAMPLE,// SubChunkSize, size of sub chunk, excluding the first two 4-byte fields
};

float square(float t, float frequency, float flip = 0.5f)
{
    float dummy;
    return std::modff(t * frequency, &dummy) < flip ? -1.0f : 1.0f;
}

float sin(float t, float frequency)
{
    return std::sinf(t * frequency * PI * 2.0f);
}

float envSqrt(float period)
{
    period = period < 0.5 ? period : 1.0f - period;
    return sqrtf(2.0f * period);
}

float envSqDrop(float period)
{
    return (1.0f - period) * (1.0f - period);
}

float envSqrtDrop(float period)
{
    return sqrtf(1.0f - period);
}

float envSq(float period)
{
    period = period < 0.5 ? period : 1.0f - period;
    return period * period * 4.0f;
}

float envAdsr(float period, float attack, float decay, float sustainLevel, float release)
{
    if (period <= attack)
    {
        return period / attack;
    }
    else if (period <= decay)
    {
        return 1.0f - (1.0f - sustainLevel) * ((period - attack) / (decay - attack));
    }
    else if (period <= release)
    {
        return sustainLevel;
    }
    else
    {
        return sustainLevel - sustainLevel * (period - release) / (1.0f - release);
    }
}

template <typename T>
void clamp(T& x, const T& min, const T& max) {
    x = x > max ? max : x;
    x = x < min ? min : x;
}

float compress(float x, float threshold, float reduction) {
    // do dynamic range compression?
    return x;
} 

int main()
{
    static int16_t buffer[BUFFER_COUNT];
    memcpy(buffer, WAV_HEADER, 44);

    std::vector<float> harmonics;
    std::vector<float> levels;
    do
    {
        {
            float harmonic;
            float level;
            while (true) {
                std::cin >> harmonic;
                if (harmonic == 0) break;
                size_t harmonicPos = harmonics.size();
                for (size_t i = 0; i < harmonicPos; ++i)
                {
                    if (std::fabsf(harmonics[i] - harmonic) < 0.0001f) {
                        harmonicPos = i;
                        break;
                    }
                }
                std::cin >> level;
                clamp(level, -1.0f, 1.0f);
                if (harmonicPos == harmonics.size())
                {
                    harmonics.push_back(harmonic);
                    levels.push_back(level);
                }
                else
                {
                    levels[harmonicPos] = level;
                }
            }
        }

        if (harmonics.size() > 0)
        {
            float baseFreq = 220.0f;
            float frequencies[4] = { 0.0f };
            frequencies[0] = baseFreq;
            frequencies[1] = baseFreq * std::powf(1.0594631f, 4);
            frequencies[2] = baseFreq * std::powf(1.0594631f, 7);
            frequencies[3] = baseFreq * std::powf(1.0594631f, 5);

            float dummy;
            for (size_t i = SAMPLE_OFFSET; i < BUFFER_COUNT; ++i)
            {
                int channel = i & 1;
                float t = 0.5f * static_cast<float>(i - SAMPLE_OFFSET) / SAMPLE_RATE;
                float period = std::modff(t / BEAT_DURATION_SEC, &dummy);
                float frequency = frequencies[static_cast<size_t>(dummy) % 4];// 220.0f * pow(1.0594631f, dummy);
                float env = envSqDrop(period);
                //float env = envAdsr(period, .1f, .3f, .2f, .7f);
                float level = (1.0f / (2.0f + harmonics.size())) * (0.6f + 0.4f * std::sinf(0.5f * t * PI + channel * PI));
                float sample = 0.0f;// level * env * sin(t, frequency);
                for (size_t i = 0; i < harmonics.size(); ++i)
                {
                    sample += level * levels[i] * env * square(t, frequency * harmonics[i]);
                }
                //sample += level * env * sin(t, frequency * 3);
                //sample += level * env * sin(t, frequency * 5);
                //sample += level * env * sin(t, frequency * 9);
                //sample += level * env * sin(t, frequency*pow(1.0594631f, 4));
                //sample += level * env * sin(t, frequency*pow(1.0594631f, 7));
                //sample += level * env * sin(t, frequency*pow(1.0594631f, 10));
                buffer[i] = static_cast<int16_t>(sample * 32767);
                clamp(buffer[i], int16_t(-32767), int16_t(32767));
            }

            PlaySound((LPCTSTR)buffer, nullptr, SND_MEMORY | SND_SYNC);
        }
    } while (harmonics.size() > 0);

    return 0;
}
