#include "Compressor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using vitaplug::lab2576c::Compressor;
using vitaplug::lab2576c::Settings;
constexpr int sampleRate = 48000;
constexpr double twoPi = 6.28318530717958647692;

void check(bool passed, const char* label)
{
    std::cout << (passed ? "PASS  " : "FAIL  ") << label << '\n';
    if (!passed)
        std::exit(EXIT_FAILURE);
}

bool near(float first, float second, float tolerance) { return std::abs(first - second) <= tolerance; }
float db(float gain) { return 20.0f * std::log10(gain); }
float gain(float levelDb) { return std::pow(10.0f, levelDb / 20.0f); }

std::vector<float> render(const std::vector<float>& signal, const Settings& settings, std::size_t block = 127)
{
    Compressor engine;
    engine.prepare(sampleRate, settings);
    auto result = signal;
    for (std::size_t i = 0; i < result.size(); i += block)
        engine.process(result.data() + i, nullptr, std::min(block, result.size() - i), settings);
    return result;
}

std::vector<float> sine(float frequency, float amplitude, int frames = sampleRate)
{
    std::vector<float> signal(static_cast<std::size_t>(frames));
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = amplitude * static_cast<float>(std::sin(twoPi * frequency * static_cast<double>(i) / sampleRate));
    return signal;
}

float rmsAtEnd(const std::vector<float>& signal)
{
    constexpr auto window = sampleRate / 5;
    auto sum = 0.0;
    for (auto i = signal.cend() - window; i != signal.cend(); ++i)
        sum += static_cast<double>(*i) * *i;
    return static_cast<float>(std::sqrt(sum / window));
}

void testUnityAndParallelMix()
{
    auto source = sine(173.0f, 0.65f);
    Settings settings;
    settings.ratio = 1.0f;
    settings.scHighPassHz = 500.0f;
    settings.scTiltDb = 6.0f;
    check(render(source, settings) == source, "1:1 unity with both sidechain filters active");

    settings.ratio = 8.0f;
    settings.inputDb = 6.0f;
    const auto wet = render(source, settings);
    settings.mix = 0.0f;
    check(render(source, settings) == source, "MIX at 0% taps the input before INPUT gain");
    settings.mix = 0.5f;
    const auto parallel = render(source, settings);
    const auto index = static_cast<std::size_t>(28000);
    check(near(parallel[index], 0.5f * (source[index] + wet[index]), 0.000002f), "MIX blends dry and wet linearly");
    settings.outputDb = 6.0f;
    const auto louder = render(source, settings);
    check(near(louder[index], parallel[index] * gain(6.0f), 0.000002f), "OUTPUT gain follows parallel MIX");
}

void testStaticRatio()
{
    Settings settings;
    settings.kneeDb = 0.0f;
    settings.thresholdDb = -24.0f;
    settings.ratio = 4.0f;
    constexpr std::array<const char*, 3> names { "VCA feed-forward static ratio", "VCA feedback static ratio", "FET feedback static ratio" };
    for (std::size_t mode = 0; mode < names.size(); ++mode)
    {
        settings.morph = mode == 2 ? 1.0f : 0.0f;
        settings.vcaFeedback = mode == 1;
        const auto quiet = render(std::vector<float>(sampleRate, gain(-16.0f)), settings).back();
        const auto loud = render(std::vector<float>(sampleRate, gain(-4.0f)), settings).back();
        check(near(db(quiet), -22.0f, 0.04f) && near(db(loud / quiet), 3.0f, 0.04f), names[mode]);
    }
}

void testMorph()
{
    auto source = sine(127.0f, 0.58f, sampleRate / 4);
    for (std::size_t i = 0; i < source.size(); ++i)
        source[i] *= (i % 2400 < 260 ? 1.0f : 0.17f);
    Settings settings;
    settings.thresholdDb = -25.0f;
    settings.attackMs = 0.14f;
    settings.ratio = 6.0f;
    settings.morph = 0.0f;
    const auto vca = render(source, settings);
    settings.morph = 1.0f;
    const auto fet = render(source, settings);
    settings.morph = 0.5f;
    const auto hybrid = render(source, settings);

    auto largestDifference = 0.0f;
    auto largestError = 0.0f;
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        largestDifference = std::max(largestDifference, std::abs(vca[i] - fet[i]));
        const auto interpolated = std::copysign(std::sqrt(vca[i] * fet[i]), source[i]);
        largestError = std::max(largestError, std::abs(hybrid[i] - interpolated));
    }

    check(largestDifference > 0.06f, "VCA and FET give different transient responses");
    check(largestError < 0.000002f, "MORPH at 50% interpolates gain in dB");
    check(hybrid == render(source, settings, 1) && hybrid == render(source, settings, source.size()),
          "identical DSP at 1, 127 and full-buffer block sizes");
}

void testStereoLink()
{
    Settings settings;
    constexpr auto loud = 0.72f;
    constexpr auto quiet = 0.04f;
    std::vector<float> left(sampleRate, loud), right(sampleRate, quiet);
    Compressor engine;
    engine.prepare(sampleRate, settings);
    engine.process(left.data(), right.data(), left.size(), settings);
    const auto linkedQuiet = right.back();
    check(near(left.back() / loud, right.back() / quiet, 0.000003f) && right.back() < quiet,
          "LINK at 100% matches the channel gains");

    settings.stereoLink = 0.0f;
    std::fill(left.begin(), left.end(), loud);
    std::fill(right.begin(), right.end(), quiet);
    engine.process(left.data(), right.data(), left.size(), settings);
    check(near(right.back(), quiet, 0.0000001f), "LINK at 0% leaves the quieter channel independent");

    settings.stereoLink = 0.5f;
    std::fill(left.begin(), left.end(), loud);
    std::fill(right.begin(), right.end(), quiet);
    engine.process(left.data(), right.data(), left.size(), settings);
    check(near(right.back(), std::sqrt(linkedQuiet * quiet), 0.000003f), "LINK at 50% interpolates gain in dB");
}

void testMorphAutomation()
{
    Settings settings;
    settings.morph = 0.0f;
    settings.thresholdDb = -18.0f;
    settings.kneeDb = 18.0f;
    settings.ratio = 12.0f;
    constexpr auto input = 0.30f;
    Compressor engine;
    engine.prepare(sampleRate, settings);
    std::vector<float> signal(sampleRate, input);
    engine.process(signal.data(), nullptr, signal.size(), settings);
    const auto start = signal.back();

    signal.assign(700, input);
    settings.morph = 1.0f;
    engine.process(signal.data(), nullptr, signal.size(), settings);
    auto maxStep = std::abs(signal.front() - start);
    for (std::size_t i = 1; i < signal.size(); ++i)
        maxStep = std::max(maxStep, std::abs(signal[i] - signal[i - 1]));

    const auto settledFet = render(std::vector<float>(sampleRate, input), settings).back();
    check(std::abs(start - settledFet) > 0.01f && maxStep < 0.00006f && near(signal.back(), settledFet, 0.000002f),
          "automated MORPH ramps to the already-settled FET detector");
}

void testDetectorFilters()
{
    Settings settings;
    settings.morph = 0.0f;
    settings.thresholdDb = -27.0f;
    settings.ratio = 6.0f;
    const auto bass = sine(45.0f, 0.7f);
    const auto high = sine(2000.0f, 0.7f);
    const auto bassFlat = rmsAtEnd(render(bass, settings));
    const auto highFlat = rmsAtEnd(render(high, settings));
    settings.scHighPassHz = 200.0f;
    check(db(rmsAtEnd(render(bass, settings)) / bassFlat) > 8.0f, "SC HPF reduces bass-triggered compression");
    check(std::abs(db(rmsAtEnd(render(high, settings)) / highFlat)) < 0.8f, "SC HPF preserves high-frequency detector response");
    settings.scHighPassHz = 0.0f;
    settings.scTiltDb = 6.0f;
    check(db(rmsAtEnd(render(bass, settings)) / bassFlat) > 4.0f, "SC TILT changes detector weighting");
}

void testProgramRelease()
{
    Settings settings;
    settings.morph = 1.0f;
    settings.attackMs = 0.02f;
    settings.releaseMs = 60.0f;
    settings.thresholdDb = -27.0f;
    const auto afterHold = [&settings](int hold)
    {
        std::vector<float> signal(static_cast<std::size_t>(hold + sampleRate / 16), 0.01f);
        std::fill_n(signal.begin(), hold, 0.85f);
        return render(signal, settings).back();
    };
    check(db(afterHold(sampleRate / 100) / afterHold(sampleRate / 2)) > 2.0f,
          "FET release slows after sustained gain reduction");
}

void testBypass()
{
    Compressor engine;
    Settings settings;
    settings.ratio = 20.0f;
    settings.thresholdDb = -44.0f;
    engine.prepare(sampleRate, settings);
    constexpr auto dry = 0.64f;
    std::vector<float> signal(sampleRate / 2, dry);
    engine.process(signal.data(), nullptr, signal.size(), settings);
    const auto previous = signal.back();
    signal.assign(721, dry);
    settings.bypass = true;
    engine.process(signal.data(), nullptr, signal.size(), settings);
    auto step = std::abs(signal.front() - previous);
    for (std::size_t i = 1; i < signal.size(); ++i)
        step = std::max(step, std::abs(signal[i] - signal[i - 1]));
    check(step < 0.003f && signal.back() == dry && signal[384] == dry, "8 ms bypass ramp reaches exact dry");
    settings.bypass = false;
    signal.assign(721, dry);
    engine.process(signal.data(), nullptr, signal.size(), settings);
    check(near(signal.back(), previous, 0.00002f), "detectors stay active during bypass");
}

void testExtremeAutomation()
{
    constexpr std::array<double, 4> rates { 44100, 48000, 96000, 192000 };
    constexpr int frames = 237;
    auto in = [](std::size_t n, int block)
    {
        const auto signal = std::sin(static_cast<float>(n) * 0.26f + static_cast<float>(block) * 0.63f);
        return n == 0 ? 1.25f : 0.92f * signal;
    };
    for (const auto rate : rates)
    {
        Compressor engine;
        Settings settings;
        settings.ratio = 20.0f;
        settings.kneeDb = 0.0f;
        settings.attackMs = 0.02f;
        settings.releaseMs = 30.0f;
        settings.thresholdDb = -60.0f;
        engine.prepare(rate, settings);
        std::array<float, frames> left {}, right {};
        for (int block = 0; block < 150; ++block)
        {
            settings.morph = static_cast<float>(block % 11) / 10.0f;
            settings.kneeDb = block % 5 == 0 ? 18.0f : 0.0f;
            settings.vcaFeedback = block % 3 == 0;
            settings.stereoLink = block % 2 == 0 ? 1.0f : 0.0f;
            settings.scHighPassHz = block % 2 == 0 ? 500.0f : 0.0f;
            for (std::size_t i = 0; i < left.size(); ++i)
            {
                left[i] = in(i, block);
                right[i] = -0.3f * left[i];
            }
            engine.process(left.data(), right.data(), left.size(), settings);
            for (std::size_t i = 0; i < left.size(); ++i)
            {
                const auto sample = in(i, block);
                if (!std::isfinite(left[i]) || !std::isfinite(right[i])
                    || std::abs(left[i]) > std::abs(sample) + 0.00001f
                    || std::abs(right[i]) > std::abs(sample * 0.3f) + 0.00001f)
                    check(false, "extreme automation stays finite and never amplifies");
            }
        }

        left.fill(0.0f);
        left[1] = std::numeric_limits<float>::quiet_NaN();
        engine.process(left.data(), nullptr, left.size(), settings);
        if (!std::all_of(left.begin(), left.end(), [](float sample) { return sample == 0.0f; }))
            check(false, "silence and a malformed sample leave finite zero output");
    }
    check(true, "44.1–192 kHz, extreme automation, silence and malformed input");
}
}

int main()
{
    testUnityAndParallelMix();
    testStaticRatio();
    testMorph();
    testMorphAutomation();
    testStereoLink();
    testDetectorFilters();
    testProgramRelease();
    testBypass();
    testExtremeAutomation();
}
