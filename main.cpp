#include <cstdint>
#include <cmath>

#include <Windows.h>

static constexpr float PI = 3.14159265f;

static constexpr uint32_t SAMPLE_RATE = 44100;

static constexpr size_t BPM = 120;
static constexpr size_t MEASURES = 2;
static constexpr size_t METER_IN_4TH = 4;
static constexpr float BEAT_DURATION_SEC = 60.0 / BPM;
static constexpr size_t BEATS_TOTAL = MEASURES * METER_IN_4TH;
static constexpr float DURATION_SEC = BEATS_TOTAL * BEAT_DURATION_SEC;
static constexpr size_t NUM_CHANNELS = 2;
static constexpr size_t BYTE_PER_SAMPLE = 2;
static constexpr size_t BITS_PER_SAMPLE = BYTE_PER_SAMPLE * 8;
static constexpr size_t SAMPLE_COUNT = static_cast<size_t>(SAMPLE_RATE * NUM_CHANNELS * DURATION_SEC);
static constexpr size_t SAMPLE_OFFSET = 44 / BYTE_PER_SAMPLE; // WAV_HEADER is 44 bytes, so 22 uint16 elements
static constexpr size_t BUFFER_COUNT = SAMPLE_OFFSET + SAMPLE_COUNT;

using namespace std;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Storage::Streams;
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <ppltasks.h>

// TODO: reference additional headers your program requires here
#include "ROApi.h"

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

float envSq(float period)
{
    period = period < 0.5 ? period : 1.0f - period;
    return period * period * 4.0f;
}

float interpolateLin(float v0, float v1, float x, float x0 = 0.0, float x1 = 1.0)
{
    return v0 + (v1 - v0) * (x - x0) / (x1 - x0);
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

void TTStest() {
    Windows::Foundation::Initialize();
    auto synthesizer = ref new SpeechSynthesizer();
    // Create a stream from the text. This will be played using a media element.
    //auto stream = synthesizer->SynthesizeTextToStreamAsync("Hallo")->GetResults();

    //auto media = ref new MediaElement();


    // The string to speak.
    String^ text = "Hello World";

    // Generate the audio stream from plain text.
    Concurrency::task<SpeechSynthesisStream ^> speakTask = Concurrency::create_task(synthesizer->SynthesizeTextToStreamAsync(text));
    speakTask.then([text](SpeechSynthesisStream ^speechStream)
    {
        auto buffer = ref new Buffer(1024*1024);
        auto writeTask = Concurrency::create_task(speechStream->WriteAsync(buffer));
        writeTask.then([buffer](unsigned int size) {
            buffer->GetHashCode();
        });

        speechStream->FlushAsync();
        // Send the stream to the media object.
        // media === MediaElement XAML object.
        //media->SetSource(speechStream, speechStream->ContentType);
        //media->AutoPlay = true;
        //media->Play();
    });

            ////SpeechSynthesisStream^ synthesisStream = synthesisStreamTask.get();
            ////
            ////// Set the source and start playing the synthesized audio stream.
            //media->AutoPlay = true;
            //media->SetSource(stream, stream->ContentType);
            //media->Play();
}

int main()
{
    TTStest();
    static int16_t buffer[BUFFER_COUNT];
    memcpy(buffer, WAV_HEADER, 44);

    float frequency = 440.0;
    float dummy;
    for (size_t i = SAMPLE_OFFSET; i < BUFFER_COUNT; ++i) {
        int channel = i & 1;
        float t = 0.5f * static_cast<float>(i - SAMPLE_OFFSET) / SAMPLE_RATE;
        float period = std::modff(t/BEAT_DURATION_SEC, &dummy);
        frequency = 220.0f * pow(1.0594631f, dummy);
        float env = envAdsr(period, .1f, .3f, .2f, .7f);
        float level = 0.6f + 0.4f * std::sinf(0.5f * t * PI + channel * PI);
        buffer[i] = static_cast<int16_t>(level * env * sin(t, frequency) * 32767);
    }

    PlaySound((LPCTSTR)buffer, nullptr, SND_MEMORY | SND_SYNC);

    return 0;
}
