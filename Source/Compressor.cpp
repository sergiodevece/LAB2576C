#include "Compressor.h"

#include <algorithm>
#include <cmath>

namespace vitaplug::lab2576c
{
namespace
{
constexpr float floorMagnitude = 1.0e-9f;
constexpr float floorPower = floorMagnitude * floorMagnitude;
constexpr double pi = 3.14159265358979323846;

float bounded(float value, float low, float high, float fallback) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

float safeSample(float value) noexcept
{
    return bounded(value, -1000000.0f, 1000000.0f, 0.0f);
}

float gainFromDb(float db) noexcept
{
    return std::exp(db * 0.11512925465f);
}

float blend(float first, float second, float amount) noexcept
{
    return first * (1.0f - amount) + second * amount;
}

float flushTiny(float value) noexcept
{
    return std::abs(value) < floorPower ? 0.0f : value;
}

float aboveKnee(float over, float width) noexcept
{
    const auto half = 0.5f * width;
    if (over <= -half)
        return 0.0f;
    if (over >= half)
        return over;

    const auto distance = over + half;
    return distance * distance / (2.0f * width);
}

float updateFeedback(float previous, float over, float slope, float width,
                     float attackWeight, float releaseWeight) noexcept
{
    const auto requested = -slope * aboveKnee(over + previous, width);
    const auto step = requested < previous ? attackWeight : releaseWeight;
    const auto memory = (1.0f - step) * previous;
    const auto loop = step * slope;
    const auto openLoopLevel = over + memory;
    const auto halfKnee = 0.5f * width;

    // Solve for the post-gain detector level within this sample. This keeps
    // the negative feedback stable at high ratios and sub-millisecond attack.
    if (openLoopLevel <= -halfKnee)
        return flushTiny(memory);
    if (openLoopLevel >= halfKnee * (1.0f + loop))
        return flushTiny(memory - loop * openLoopLevel / (1.0f + loop));

    const auto offset = openLoopLevel + halfKnee;
    const auto distance = 2.0f * offset / (1.0f + std::sqrt(1.0f + 2.0f * loop * offset / width));
    return flushTiny(memory - loop * distance * distance / (2.0f * width));
}
}

void Compressor::Ramp::reset(float value) noexcept
{
    current = target = value;
    step = 0.0f;
    remaining = 0;
}

void Compressor::Ramp::setTarget(float value, int samples) noexcept
{
    if (target == value)
        return;

    target = value;
    remaining = samples;
    step = (target - current) / static_cast<float>(samples);
}

float Compressor::Ramp::advance() noexcept
{
    if (remaining > 0 && --remaining > 0)
        current += step;
    else
        current = target;

    return current;
}

void Compressor::prepare(double sampleRateHz, const Settings& settings) noexcept
{
    sampleRate = std::isfinite(sampleRateHz) && sampleRateHz >= 1000.0 && sampleRateHz <= 768000.0
        ? sampleRateHz : 48000.0;
    smoothingSamples = static_cast<int>(std::round(0.012 * sampleRate));
    bypassSamples = static_cast<int>(std::round(0.008 * sampleRate));
    rmsStep = timeStep(3.0f);
    loadStep = timeStep(110.0f);
    tiltStep = filterStep(850.0f);
    channels = {};
    meters = {};

    const auto controls = convert(settings);
    for (std::size_t i = 0; i < ramps.size(); ++i)
        ramps[i].reset(controls[i]);
}

float Compressor::timeStep(float ms) const noexcept
{
    return static_cast<float>(-std::expm1(-1000.0 / (sampleRate * ms)));
}

float Compressor::filterStep(float hz) const noexcept
{
    const auto angular = pi * std::min(static_cast<double>(hz), 0.45 * sampleRate) / sampleRate;
    const auto tangent = std::tan(angular);
    return static_cast<float>(tangent / (1.0 + tangent));
}

Compressor::Controls Compressor::convert(const Settings& settings) const noexcept
{
    const auto hpfHz = bounded(settings.scHighPassHz, 0.0f, 500.0f, 0.0f);
    Controls controls {};
    controls[inputGain] = gainFromDb(bounded(settings.inputDb, -24.0f, 24.0f, 0.0f));
    controls[threshold] = bounded(settings.thresholdDb, -60.0f, 0.0f, -18.0f);
    controls[ratio] = bounded(settings.ratio, 1.0f, 20.0f, 4.0f);
    controls[attack] = timeStep(bounded(settings.attackMs, 0.02f, 100.0f, 3.0f));
    controls[release] = timeStep(bounded(settings.releaseMs, 30.0f, 3000.0f, 120.0f));
    controls[knee] = bounded(settings.kneeDb, 0.0f, 18.0f, 6.0f);
    controls[morph] = bounded(settings.morph, 0.0f, 1.0f, 0.5f);
    controls[scHpf] = filterStep(hpfHz > 0.0f ? hpfHz : 20.0f);
    controls[scHpfMix] = hpfHz > 0.0f ? 1.0f : 0.0f;
    controls[scTilt] = 1.0f - gainFromDb(-bounded(settings.scTiltDb, 0.0f, 6.0f, 0.0f));
    controls[link] = bounded(settings.stereoLink, 0.0f, 1.0f, 1.0f);
    controls[mix] = bounded(settings.mix, 0.0f, 1.0f, 1.0f);
    controls[outputGain] = gainFromDb(bounded(settings.outputDb, -24.0f, 24.0f, 0.0f));
    controls[feedbackMix] = settings.vcaFeedback ? 1.0f : 0.0f;
    controls[enabled] = settings.bypass ? 0.0f : 1.0f;
    return controls;
}

float Compressor::detect(Channel& state, float wet, const Controls& values) const noexcept
{
    const auto hpfDelta = values[scHpf] * (wet - state.keyLow);
    const auto hpfLow = state.keyLow + hpfDelta;
    state.keyLow = flushTiny(hpfLow + hpfDelta);

    auto key = wet - values[scHpfMix] * hpfLow;
    const auto tiltDelta = tiltStep * (key - state.tiltLow);
    const auto tiltLow = state.tiltLow + tiltDelta;
    state.tiltLow = flushTiny(tiltLow + tiltDelta);
    key -= values[scTilt] * tiltLow;

    state.rmsPower = flushTiny(state.rmsPower + rmsStep * (key * key - state.rmsPower));
    const auto rmsOver = 10.0f * std::log10(std::max(state.rmsPower, floorPower)) - values[threshold];
    const auto peakOver = 20.0f * std::log10(std::max(std::abs(key), floorMagnitude)) - values[threshold];
    const auto slope = values[ratio] - 1.0f;

    const auto forwardTarget = -slope / values[ratio] * aboveKnee(rmsOver, values[knee]);
    const auto forwardStep = forwardTarget < state.vcaForwardDb ? values[attack] : values[release];
    state.vcaForwardDb = flushTiny(state.vcaForwardDb + forwardStep * (forwardTarget - state.vcaForwardDb));
    state.vcaFeedbackDb = updateFeedback(state.vcaFeedbackDb, rmsOver, slope, values[knee],
                                         values[attack], values[release]);

    const auto fetRelease = values[release] / (1.0f + 2.0f * state.sustainedReduction);
    state.fetFeedbackDb = updateFeedback(state.fetFeedbackDb, peakOver, slope, 0.4f * values[knee],
                                         values[attack], fetRelease);
    const auto load = std::clamp(-state.fetFeedbackDb / 12.0f, 0.0f, 1.0f);
    state.sustainedReduction = flushTiny(state.sustainedReduction + loadStep * (load - state.sustainedReduction));

    const auto vcaDb = blend(state.vcaForwardDb, state.vcaFeedbackDb, values[feedbackMix]);
    return blend(vcaDb, state.fetFeedbackDb, values[morph]);
}

void Compressor::process(float* left, float* right, std::size_t frames, const Settings& settings) noexcept
{
    if (left == nullptr || frames == 0)
        return;

    const auto targets = convert(settings);
    for (std::size_t i = 0; i < ramps.size(); ++i)
        ramps[i].setTarget(targets[i], i == enabled ? bypassSamples : smoothingSamples);

    const auto stereo = right != nullptr;
    MeterLevels block {};
    Controls values {};
    for (std::size_t i = 0; i < frames; ++i)
    {
        for (std::size_t control = 0; control < ramps.size(); ++control)
            values[control] = ramps[control].advance();

        const auto dryL = safeSample(left[i]);
        const auto wetL = dryL * values[inputGain];
        auto reductionL = detect(channels[0], wetL, values);

        if (stereo)
        {
            const auto dryR = safeSample(right[i]);
            const auto wetR = dryR * values[inputGain];
            auto reductionR = detect(channels[1], wetR, values);
            const auto strongest = std::min(reductionL, reductionR);
            reductionL = blend(reductionL, strongest, values[link]);
            reductionR = blend(reductionR, strongest, values[link]);

            const auto activeR = values[outputGain] * blend(dryR, wetR * gainFromDb(reductionR), values[mix]);
            right[i] = blend(dryR, activeR, values[enabled]);
            block.inputPeak = std::max(block.inputPeak, std::abs(dryR));
            block.outputPeak = std::max(block.outputPeak, std::abs(right[i]));
            block.reductionDb = std::max(block.reductionDb, -reductionR * values[enabled]);
        }

        const auto activeL = values[outputGain] * blend(dryL, wetL * gainFromDb(reductionL), values[mix]);
        left[i] = blend(dryL, activeL, values[enabled]);
        block.inputPeak = std::max(block.inputPeak, std::abs(dryL));
        block.outputPeak = std::max(block.outputPeak, std::abs(left[i]));
        block.reductionDb = std::max(block.reductionDb, -reductionL * values[enabled]);
    }

    const auto meterDecay = static_cast<float>(std::exp(-static_cast<double>(frames) / (0.32 * sampleRate)));
    meters.inputPeak = std::max(block.inputPeak, flushTiny(meters.inputPeak * meterDecay));
    meters.outputPeak = std::max(block.outputPeak, flushTiny(meters.outputPeak * meterDecay));
    meters.reductionDb = std::max(block.reductionDb, flushTiny(meters.reductionDb * meterDecay));
}
}
