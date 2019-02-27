#include "synth.h"

#include "sound.h"

#include <cstdint>
#include <cmath>
#include <cstring>

#include <Windows.h>

static constexpr float PI = 3.14159265f;

static constexpr uint32_t SAMPLE_RATE = 44'100;

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

template <typename T>
void clamp(T& x, const T& min, const T& max)
{
    x = x > max ? max : x;
    x = x < min ? min : x;
}

float interpolate(float v0, float v1, float x, float x0 = 0.0, float x1 = 1.0)
{
    float fac = (x - x0) / (x1 - x0);
    return (1.0f - fac) * v0 + fac * v1;
}

float rand(float x)
{
    float dummy;
    return std::modff(std::sin(x*2357911.13f)*1113171.9f, &dummy);
}

static float noise_buffer[SAMPLE_RATE];

void init_noise_buffer()
{
    static bool initialized = false;
    if (!initialized) {
        std::srand(42);

        for (size_t i = 0; i < SAMPLE_RATE; ++i)
        {
            noise_buffer[i] = -1.0f + 2.0f * (static_cast<float>(std::rand()) / RAND_MAX);
        }

        initialized = true;
    }
}

float env_sqrt(float period)
{
    period = period < 0.5 ? period : 1.0f - period;
    return sqrtf(2.0f * period);
}

float env_sq(float period)
{
    period = period < 0.5 ? period : 1.0f - period;
    return period * period * 4.0f;
}

float env_adsr(float period, float attack, float decay, float sustainLevel, float release)
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

float square(float t, float frequency, float phase = 0.0f)
{
    float flip = 0.5f;
    float dummy;
    return std::modff(t * frequency + phase, &dummy) < flip ? -1.0f : 1.0f;
}

float sin(float t, float frequency, float phase = 0.0f)
{
    return std::sinf(t * frequency * PI * 2.0f + phase * 2.0f * PI);
}

float silence(float /*t*/, float /*freq*/)
{
    return 0.0f;
}

float noise(float t, float freq, float phase = 0.0f)
{
    float noiseT = t * freq + phase * freq;
    size_t i = static_cast<size_t>(noiseT);
    float fac = noiseT - i;
    float n0 = noise_buffer[i % SAMPLE_RATE];
    float n1 = noise_buffer[(i + 1) % SAMPLE_RATE];

    return interpolate(n0, n1, fac);
}

float kick(float t, float startFreq)
{
    float env = env_adsr(t, 0.0f, 0.0f, 1.0f, 0.95f);

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
    float env = env_adsr(t, 0.0f, 0.0f, 1.0f, 0.95f);

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
    float env = env_adsr(t, 0.0f, 0.0f, 1.0f, 0.95f);

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
    float env = env_adsr(t, 0.1f, 0.3f, 0.5f, 0.9f);
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
    init_noise_buffer();
    sound_open_device(SAMPLE_RATE, BYTES_PER_SAMPLE, NUM_CHANNELS);
}

void synth_deinit()
{
    sound_close_device();
}

using sample_func = float(*)(float t, float base_freq);
static constexpr sample_func samples[] = {
    silence,
    kick,
    snare,
    hihat,
    annoying,
    [](float t, float freq) { return noise(t, freq); }
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

using base_sound_func = float(*)(float t, float freq, float phase);
static constexpr base_sound_func base_sounds[] = {
    sin,
    square,
    noise,
};

using modifier_func = float(*)(float t);
static constexpr modifier_func base_modifiers[] = {
    [](float /*t*/) -> float { return 1.0f; },
    env_sq,
    env_sqrt,
    // exp(-x) is 0.01 at x=4.60517
    [](float t) -> float { return (std::expf(-t*4.60517f) - 0.01f) / 0.99f; },
};

void synth_update_generated_sound(int16_t* const sound_buffer, const sound_desc* const sound_desc, const uint8_t num_sound_descs)
{
    size_t sample_count = static_cast<size_t>(SAMPLE_RATE * BEAT_DURATION_SEC);
    size_t buffer_count = sample_count * NUM_CHANNELS;
    std::memset(sound_buffer, 0, buffer_count * BYTES_PER_SAMPLE);

    for (uint8_t i = 0; i < num_sound_descs; ++i)
    {
        float phase = 0.0f;
        float t_inc = 1.0f / SAMPLE_RATE;
        for (size_t s = 0; s < sample_count; ++s)
        {
            float t = static_cast<float>(s) / sample_count;

            float frequency_modifier_t = interpolate(0.0f, 1.0f, t, sound_desc[i].frequency_modifier_params.begin, sound_desc[i].frequency_modifier_params.end);
            clamp(frequency_modifier_t, 0.0f, 1.0f);
            float frequency = interpolate(sound_desc[i].frequency_min, sound_desc[i].frequency, base_modifiers[sound_desc[i].frequency_modifier_id](frequency_modifier_t));

            float amplitude_modifier_t = interpolate(0.0f, 1.0f, t, sound_desc[i].amplitude_modifier_params.begin, sound_desc[i].amplitude_modifier_params.end);
            clamp(amplitude_modifier_t, 0.0f, 1.0f);
            float amplitude = interpolate(sound_desc[i].amplitude_min, sound_desc[i].amplitude, base_modifiers[sound_desc[i].amplitude_modifier_id](amplitude_modifier_t));

            float sample = amplitude * base_sounds[sound_desc[i].base_sound_id](0.0f, frequency, phase);

            for (uint8_t c = 0; c < NUM_CHANNELS; ++c)
            {
                sound_buffer[s * NUM_CHANNELS + c] += static_cast<int16_t>(sample * 32767);
            }
            phase += t_inc * frequency;
        }
    }
}

int16_t* synth_generate_sound(const sound_desc* const sound_desc, const uint8_t num_sound_descs)
{
    size_t sample_count = static_cast<size_t>(SAMPLE_RATE * BEAT_DURATION_SEC);
    size_t buffer_count = sample_count * NUM_CHANNELS;
    int16_t* sound_buffer = new int16_t[buffer_count];
    synth_update_generated_sound(sound_buffer, sound_desc, num_sound_descs);
    return sound_buffer;
}

void synth_queue_track(int16_t* track_buffer, const uint32_t loop_count)
{
    synth_play(track_buffer, SAMPLES_PER_TRACK * NUM_CHANNELS * BYTES_PER_SAMPLE, loop_count);
}

void synth_queue_sound(int16_t* sound_buffer, const uint32_t loop_count)
{
    synth_play(sound_buffer, static_cast<uint32_t>(SAMPLE_RATE * BEAT_DURATION_SEC * NUM_CHANNELS * BYTES_PER_SAMPLE), loop_count);
}

void synth_play(int16_t* buffer, const uint32_t buffer_size, const uint32_t loop_count)
{
    sound_queue_buffer(buffer, buffer_size, loop_count);
}

void synth_end_current_loop()
{
    sound_end_current_loop();
}