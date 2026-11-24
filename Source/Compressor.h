#pragma once

#include <array>
#include <cstddef>

namespace vitaplug::lab2576c
{
struct Settings
{
    float inputDb = 0.0f;
    float thresholdDb = -18.0f;
    float ratio = 4.0f;
    float attackMs = 3.0f;
    float releaseMs = 120.0f;
    float kneeDb = 6.0f;
    float morph = 0.5f;
    float scHighPassHz = 0.0f;
    float scTiltDb = 0.0f;
    float stereoLink = 1.0f;
    float mix = 1.0f;
    float outputDb = 0.0f;
    bool vcaFeedback = false;
    bool bypass = false;
};

struct MeterLevels
{
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    float reductionDb = 0.0f;
};

class Compressor final
{
public:
    void prepare(double sampleRateHz, const Settings& initialSettings) noexcept;
    void process(float* left, float* right, std::size_t frames, const Settings& settings) noexcept;
    MeterLevels meterLevels() const noexcept { return meters; }

private:
    struct Ramp
    {
        void reset(float value) noexcept;
        void setTarget(float value, int samples) noexcept;
        float advance() noexcept;

        float current = 0.0f;
        float target = 0.0f;
        float step = 0.0f;
        int remaining = 0;
    };

    struct Channel
    {
        float keyLow = 0.0f;
        float tiltLow = 0.0f;
        float rmsPower = 0.0f;
        float vcaForwardDb = 0.0f;
        float vcaFeedbackDb = 0.0f;
        float fetFeedbackDb = 0.0f;
        float sustainedReduction = 0.0f;
    };

    enum Control : std::size_t
    {
        inputGain, threshold, ratio, attack, release, knee, morph, scHpf,
        scHpfMix, scTilt, link, mix, outputGain, feedbackMix, enabled, controlCount
    };

    using Controls = std::array<float, controlCount>;

    Controls convert(const Settings&) const noexcept;
    float detect(Channel&, float wet, const Controls&) const noexcept;
    float timeStep(float ms) const noexcept;
    float filterStep(float hz) const noexcept;

    double sampleRate = 48000.0;
    int smoothingSamples = 576;
    int bypassSamples = 384;
    float rmsStep = 0.0f;
    float tiltStep = 0.0f;
    float loadStep = 0.0f;
    std::array<Ramp, controlCount> ramps {};
    std::array<Channel, 2> channels {};
    MeterLevels meters {};
};
}
