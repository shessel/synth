#include "synth.h"

#include "sound.h"

#include <cstdint>
#include <cmath>

#include <Windows.h>

static constexpr float PI = 3.14159265f;

static constexpr uint32_t SAMPLE_RATE = 44100;

static constexpr size_t BPM = 120;
static constexpr size_t MEASURES = 1;
static constexpr size_t METER_IN_4TH = 1;
static constexpr float BEAT_DURATION_SEC = 60.0 / BPM;
static constexpr size_t BEATS_TOTAL = MEASURES * METER_IN_4TH;
static constexpr float DURATION_SEC = BEATS_TOTAL * BEAT_DURATION_SEC;
static constexpr size_t NUM_CHANNELS = 2;
static constexpr size_t BYTES_PER_SAMPLE = 2;
static constexpr size_t BITS_PER_SAMPLE = BYTES_PER_SAMPLE * 8;
static constexpr size_t SAMPLES_PER_ROW = static_cast<size_t>(SAMPLE_RATE * BEAT_DURATION_SEC);
static constexpr size_t SAMPLES_PER_TRACK = ROWS_PER_TRACK * SAMPLES_PER_ROW;
static constexpr size_t SAMPLE_COUNT = static_cast<size_t>(SAMPLE_RATE * NUM_CHANNELS * DURATION_SEC);
static constexpr size_t SAMPLE_OFFSET = 0;// 44 / BYTES_PER_SAMPLE; // WAV_HEADER is 44 bytes, so 22 uint16 elements
static constexpr size_t BUFFER_COUNT = SAMPLE_OFFSET + SAMPLE_COUNT;

static float noise_buffer[SAMPLE_RATE];

// platform is little-endian, so least significant byte comes first in memory
// => bytes are reversed when specifying multiple bytes as one int
static constexpr uint32_t WAV_HEADER[] = {
    // Main chunk
    0x46464952, // ChunkId = "RIFF"
    BUFFER_COUNT * BYTES_PER_SAMPLE - 8,// ChunkSize, size of the main chunk, excluding the first two 4-byte fields
    0x45564157, // Format = "WAVE"
    // WAVE format has 2 sub chunks, fmt and data

    // Sub chunk fmt
    0x20746d66, // SubChunkId = "fmt "
    16, // SubChunkSize, size of sub chunk, excluding the first two 4-byte fields
    NUM_CHANNELS << 16 | 1, // 2 bytes AudioFormat = 1 for PCM, 2 bytes NumChannels,
    SAMPLE_RATE, // SampleRate
    SAMPLE_RATE * NUM_CHANNELS * BYTES_PER_SAMPLE, // ByteRate = SampleRate * NumChannels * BytesPerSample
    BITS_PER_SAMPLE << 16 | NUM_CHANNELS * BYTES_PER_SAMPLE, // 2 bytes BlockAlign = NumChannels * BytesPerSample, 2 bytes BitsPerSample

    // Sub chunk data
    0x61746164, // SubChunkId = "data"
    SAMPLE_COUNT * BYTES_PER_SAMPLE,// SubChunkSize, size of sub chunk, excluding the first two 4-byte fields
};

template <typename T>
void clamp(T& x, const T& min, const T& max)
{
    x = x > max ? max : x;
    x = x < min ? min : x;
}

float interpolate(float v0, float v1, float x, float x0 = 0.0, float x1 = 1.0)
{
    float fac = x / (x1 - x0);
    return (1.0f - fac) * v0 + fac * v1;
}

float rand(float x)
{
    float dummy;
    return std::modff(std::sin(x*2357911.13f)*1113171.9f, &dummy);
}

void initNoiseBuffer()
{
    static bool initialized = false;
    if (!initialized) {
        std::srand(42);
    }

    for (size_t i = 0; i < SAMPLE_RATE; ++i)
    {
        noise_buffer[i] = -1.0f + 2.0f * (static_cast<float>(std::rand()) / RAND_MAX);
    }

    initialized = true;
}

float envSqrt(float period)
{
    period = period < 0.5 ? period : 1.0f - period;
    return sqrtf(2.0f * period);
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
        return sustainLevel - sustainLevel * (period - release) / max(1.0f - release, 0.00001f);
    }
}

float compress(float x, float threshold, float reduction)
{
    static float peak = 0.0f;
    peak = std::fmaxf(std::fabsf(x), peak);
    x = peak > threshold ? x * (1.0f / reduction) : x;
    peak -= 1.0f / (2.0f * 44100.0f);
    return x;
}

float square(float t, float frequency, float flip = 0.5f)
{
    float dummy;
    return std::modff(t * frequency, &dummy) < flip ? -1.0f : 1.0f;
}

float sin(float t, float frequency)
{
    return std::sinf(t * frequency * PI * 2.0f);
}

float silence(float /*t*/, float /*freq*/)
{
    return 0.0f;
}

float noise(float t, float freq)
{
    clamp(t, 0.0f, 1.0f);
    clamp(freq, 0.0f, static_cast<float>(SAMPLE_RATE));
    float noiseT = t * freq;
    size_t i = static_cast<size_t>(noiseT);
    float fac = noiseT - i;
    float n0 = noise_buffer[i];
    float n1 = noise_buffer[(i + 1) % SAMPLE_RATE];

    return interpolate(n0, n1, fac);
}

float kick(float t, float startFreq)
{
    float env = envAdsr(t, 0.0f, 0.0f, 1.0f, 0.95f);

    float lowBoomEnv = std::expf(-1.5f*t);
    float freqFalloff = std::expf(-0.45f*t);
    float lowBoom = lowBoomEnv * std::sinf(2.0f * PI * startFreq * freqFalloff);

    float punchT = t * 400.0f;
    float punchEnv = std::expf(-0.95f*punchT);
    clamp(punchT, 0.0f, 1.0f);
    float punch = punchEnv * noise(t, 240.0) * 0.7f;

    float slapT = t * 400.0f;
    float slapFalloff = std::expf(-0.25f*slapT);
    clamp(slapT, 0.0f, 1.0f);
    float slap = slapFalloff * noise(t, 5000.0f) * 0.3f;

    return env * (lowBoom + punch + slap);
}

float hihat(float t, float startFreq)
{
    float env = envAdsr(t, 0.0f, 0.0f, 1.0f, 0.95f);

    float punchT = t * 80.0f;
    float punchEnv = std::expf(-0.65f*punchT);
    clamp(punchT, 0.0f, 1.0f);
    float punch = punchEnv * noise(t, startFreq) * 0.7f;

    float slapT = t * 200.0f;
    float slapFalloff = std::expf(-0.45f*slapT);
    clamp(slapT, 0.0f, 1.0f);
    float slap = slapFalloff * noise(t, 2000.0f) * 0.3f;

    return env * (punch + slap);
}

float snare(float t, float startFreq)
{
    float env = envAdsr(t, 0.0f, 0.0f, 1.0f, 0.95f);

    float punchEnv = std::expf(-11.0f*t);
    float punch = punchEnv * noise(t, startFreq);

    float slapT = t * 200.0f;
    float slapFalloff = std::expf(-0.45f*slapT);
    float slap = slapFalloff * noise(t, 6000.0f) * 0.3f;

    float slap2T = t * 100.0f;
    float slap2Falloff = std::expf(-0.45f*slap2T);
    float slap2 = slap2Falloff * noise(t, 8000.0f) * 0.3f;

    return env * (punch + slap + slap2);
}

static constexpr float semitone = 1.0594631f;
static constexpr float note_multipliers[] = {
    1.0f,
    semitone,
    semitone * semitone,
    semitone * semitone * semitone,
    semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone,
    semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone * semitone,
};

float annoying(float t, float startFreq)
{
    float env = envAdsr(t, 0.1f, 0.3f, 0.5f, 0.9f);
    float sample = 0.9f * square(t, startFreq);
    sample += 1.0f * square(t, startFreq * note_multipliers[3]);
    sample += 0.8f * square(t, startFreq * note_multipliers[7]);
    sample += 0.6f * square(t, startFreq * 2.0f);
    //sample += 0.3f * square(t, startFreq * 2.1f);
    //sample += 0.2f * square(t, startFreq * 3.9f);
    sample += 0.8f * square(t, startFreq * 4.0f);
    sample /= 6.0f;
    clamp(sample, 0.0f, 1.0f);
    return env * sample;
}

void synth_init()
{
    initNoiseBuffer();
    sound_open_device(SAMPLE_RATE, BYTES_PER_SAMPLE, NUM_CHANNELS);
}

void synth_deinit()
{
    sound_close_device();
}

static int16_t s_buffer[BUFFER_COUNT];

void synth_generate(ADSR adsr)
{
    float frequency = 440.0;
    float dummy;
    for (size_t i = 0; i < BUFFER_COUNT; ++i) {
        float t = static_cast<float>(i / NUM_CHANNELS) / SAMPLE_RATE;
        float period = std::modff(t / BEAT_DURATION_SEC, &dummy);
        frequency = 220.0f * pow(1.0594631f, dummy);
        float env = envAdsr(period, adsr.a, adsr.d, adsr.s, adsr.r);
        float level = (1.0f / 3.0f);
        float sample = level * env * kick(period, 60.0f);
        //sample = compress(sample, 0.8f, 4.0f);
        s_buffer[i] = static_cast<int16_t>(sample * 32767);
        clamp(s_buffer[i], int16_t(-32767), int16_t(32767));
    }
}

using sample_func = float(*)(float t, float freq);
static constexpr sample_func samples[] = {
    silence,
    kick,
    snare,
    hihat,
    annoying,
    noise,
};

int16_t* synth_generate_track(const track& track)
{
    const size_t buffer_size = static_cast<size_t>(SAMPLES_PER_TRACK * NUM_CHANNELS);
    int16_t *track_buffer = new int16_t[buffer_size];
    int16_t *current_sample = track_buffer;
    for (const auto& row : track.rows)
    {
        for (size_t i = 0; i < SAMPLES_PER_ROW; ++i)
        {
            float t = static_cast<float>(i) / SAMPLES_PER_ROW;
            for (size_t c = 0; c < NUM_CHANNELS; ++c)
            {
                *current_sample++ = static_cast<int16_t>(samples[row.sample_id](t, row.note) * 32767);
            }
        }
    }

    return track_buffer;
}

void synth_queue_track(int16_t* track_buffer, uint32_t loop_count)
{
    synth_play(track_buffer, SAMPLES_PER_TRACK * NUM_CHANNELS * BYTES_PER_SAMPLE, loop_count);
}

void synth_play(int16_t* buffer, uint32_t buffer_size, uint32_t loop_count)
{
    if (!buffer)
    {
        buffer = s_buffer;
        buffer_size = BUFFER_COUNT * BYTES_PER_SAMPLE;
    }
    sound_queue_buffer(buffer, buffer_size, loop_count);
}