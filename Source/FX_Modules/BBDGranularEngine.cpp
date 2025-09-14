//================================================================================
// File: FX_Modules/BBDGranularEngine.cpp
//================================================================================
#include "BBDGranularEngine.h"

BBDGranularEngine::BBDGranularEngine()
{
    auto timeSeed = static_cast<std::uintptr_t>(juce::Time::currentTimeMillis());
    auto addressSeed = reinterpret_cast<std::uintptr_t>(this);
    randomEngine.seed(static_cast<unsigned int>(timeSeed ^ addressSeed));
    noiseGen.setType(DSPUtils::NoiseGenerator::NoiseType::Pink);
}

void BBDGranularEngine::prepare(const juce::dsp::ProcessSpec& spec, const Config& newConfig, int maxBufferSizeSamples)
{
    sampleRate = spec.sampleRate;
    numChannels = (int)spec.numChannels;
    config = newConfig;

    captureBuffer.prepare(spec, maxBufferSizeSamples);

    grains.resize(MAX_GRAINS);
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    for (auto& grain : grains)
    {
        grain.filterL.prepare(monoSpec);
        grain.filterR.prepare(monoSpec);
        grain.filterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        grain.filterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    reset();
}

void BBDGranularEngine::reset()
{
    captureBuffer.reset();
    for (auto& grain : grains)
    {
        grain.isActive = false;
        grain.filterL.reset();
        grain.filterR.reset();
    }
    samplesUntilNextGrain = 0.0f;
}

void BBDGranularEngine::capture(const juce::dsp::AudioBlock<float>& inputBlock)
{
    captureBuffer.write(inputBlock);
}

float BBDGranularEngine::Grain::applyTukeyWindow(float phase)
{
    const float alpha = 0.5f;
    if (phase < alpha / 2.0f)
        return 0.5f * (1.0f + std::cos(juce::MathConstants<float>::twoPi / alpha * (phase - alpha / 2.0f)));
    if (phase > 1.0f - alpha / 2.0f)
        return 0.5f * (1.0f + std::cos(juce::MathConstants<float>::twoPi / alpha * (phase - 1.0f + alpha / 2.0f)));
    return 1.0f;
}

void BBDGranularEngine::spawnGrain(float timeMs, float spread, float age)
{
    for (auto& grain : grains)
    {
        if (!grain.isActive)
        {
            grain.isActive = true;

            float durationMs = juce::jmap(distribution(randomEngine), config.minDurationMs, config.maxDurationMs);
            grain.durationSamples = static_cast<int>(sampleRate * durationMs / 1000.0);
            grain.grainPhase = 0.0f;

            float baseDelaySamples = (float)(sampleRate * timeMs / 1000.0);
            float jitter = (distribution(randomEngine) * 2.0f - 1.0f) * spread * baseDelaySamples * 0.5f;
            float actualDelaySamples = juce::jmax(10.0f, baseDelaySamples + jitter);

            int bufferSize = captureBuffer.getSize();
            if (bufferSize == 0) { grain.isActive = false; return; }

            grain.bufferReadPosition = (float)captureBuffer.getWritePosition() - actualDelaySamples;

            float normalizedTime = actualDelaySamples / (float)(sampleRate * 0.05);
            grain.pitchRatio = 1.0f / normalizedTime;
            grain.pitchRatio = juce::jlimit(0.1f, 5.0f, grain.pitchRatio);

            float baseCutoff = config.baseCutoffHz;
            baseCutoff *= (1.0f - age * 0.7f);

            float nyquist = (float)sampleRate * 0.5f;
            float aaCutoff = nyquist;

            if (grain.pitchRatio > 1.0f)
            {
                aaCutoff = nyquist / grain.pitchRatio;
            }
            else if (grain.pitchRatio < 1.0f)
            {
                aaCutoff = nyquist * grain.pitchRatio;
            }

            float cutoff = juce::jmin(baseCutoff, aaCutoff * 0.95f);
            cutoff = juce::jlimit(50.0f, nyquist - 50.0f, cutoff);

            grain.filterL.setCutoffFrequency(cutoff);
            grain.filterR.setCutoffFrequency(cutoff);
            grain.filterL.setResonance(0.707f);
            grain.filterR.setResonance(0.707f);
            grain.filterL.reset();
            grain.filterR.reset();

            grain.noiseLevel = age * config.noiseAmount;
            grain.amplitude = 0.7f + distribution(randomEngine) * 0.3f;
            grain.pan = distribution(randomEngine);

            return;
        }
    }
}

void BBDGranularEngine::process(juce::dsp::AudioBlock<float>& outputBlock, float density, float timeMs, float spread, float age)
{
    juce::ScopedNoDenormals noDenormals;

    int numSamples = (int)outputBlock.getNumSamples();
    float spawnRateHz = juce::jmap(density, 0.0f, config.spawnRateHzMax);
    if (spawnRateHz < 0.1f) spawnRateHz = 0.1f;
    float spawnIntervalSamples = (float)sampleRate / spawnRateHz;

    for (int i = 0; i < numSamples; ++i)
    {
        samplesUntilNextGrain -= 1.0f;
        if (samplesUntilNextGrain <= 0.0f)
        {
            spawnGrain(timeMs, spread, age);
            samplesUntilNextGrain += spawnIntervalSamples * (0.7f + distribution(randomEngine) * 0.6f);
        }

        for (auto& grain : grains)
        {
            if (grain.isActive)
            {
                float phase = grain.grainPhase / (float)grain.durationSamples;
                if (phase >= 1.0f)
                {
                    grain.isActive = false;
                    continue;
                }

                float window = Grain::applyTukeyWindow(phase);
                float gainL = window * std::cos(grain.pan * juce::MathConstants<float>::halfPi);
                float gainR = window * std::sin(grain.pan * juce::MathConstants<float>::halfPi);

                float sampleL = 0.0f, sampleR = 0.0f;

                if (numChannels > 0)
                    sampleL = captureBuffer.read(0, grain.bufferReadPosition);
                if (numChannels > 1)
                    sampleR = captureBuffer.read(1, grain.bufferReadPosition);
                else
                    sampleR = sampleL;

                sampleL += noiseGen.getNextSample() * grain.noiseLevel;
                sampleR += noiseGen.getNextSample() * grain.noiseLevel;

                float drive = config.saturationDrive * (1.0f + age * 0.5f);
                sampleL = DSPUtils::fastTanh(sampleL * drive);
                sampleR = DSPUtils::fastTanh(sampleR * drive);

                // JUCE 8 FIX (C2660): Pass channel index 0 as required by JUCE 8 API for mono filters.
                sampleL = grain.filterL.processSample(0, sampleL);
                sampleR = grain.filterR.processSample(0, sampleR);

                outputBlock.addSample(0, i, sampleL * gainL * grain.amplitude);
                if (numChannels > 1)
                    outputBlock.addSample(1, i, sampleR * gainR * grain.amplitude);

                grain.grainPhase += 1.0f;
                grain.bufferReadPosition += grain.pitchRatio;
            }
        }
    }
}