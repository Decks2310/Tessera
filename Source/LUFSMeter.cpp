//================================================================================
// File: LUFSMeter.cpp
//================================================================================
#include "LUFSMeter.h"

LUFSMeter::LUFSMeter() = default;

void LUFSMeter::prepare(const juce::dsp::ProcessSpec& spec)
{
    // K-Weighting Filter Coefficients
    // Stage 1: High-shelf
    stage1Filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(spec.sampleRate, 1500.0f, 0.5f, 4.0f);
    // Stage 2: High-pass
    stage2Filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 38.0f);

    stage1Filter.prepare(spec);
    stage2Filter.prepare(spec);
    weightedBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
    // Integration window setup
    momentarySamples = (int)(spec.sampleRate * momentaryIntegrationMs / 1000.0);

    reset();
}

void LUFSMeter::reset()
{
    stage1Filter.reset();
    stage2Filter.reset();
    momentaryBlockLoudness.clear();
    currentMomentaryLoudness = -144.0f;
}

void LUFSMeter::process(const juce::dsp::AudioBlock<float>& block)
{
    // Use a scoped copy of the block to avoid modifying the original
    juce::dsp::AudioBlock<float> weightedBlock(weightedBuffer);
    // Safety check: Ensure the subBlock size matches the input block size and buffer capacity
    auto numSamples = juce::jmin((size_t)weightedBuffer.getNumSamples(), block.getNumSamples());
    if (numSamples == 0) return;

    auto subBlock = weightedBlock.getSubBlock(0, numSamples);
    subBlock.copyFrom(block.getSubBlock(0, numSamples));

    applyKWeighting(subBlock);
    updateGatedLoudness(subBlock);
}

void LUFSMeter::applyKWeighting(juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> context(block);
    stage1Filter.process(context);
    stage2Filter.process(context);
}

void LUFSMeter::updateGatedLoudness(const juce::dsp::AudioBlock<float>& block)
{
    int numSamples = (int)block.getNumSamples();
    int numChannels = (int)block.getNumChannels();
    // Handle case where there are no channels
    if (numChannels == 0) return;
    for (int i = 0; i < numSamples; ++i)
    {
        double sumSquares = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float sample = block.getSample((size_t)ch, (size_t)i);
            sumSquares += (double)(sample * sample);
        }
        // Calculate mean square for the sample across channels
        float avgSquare = (float)sumSquares / (float)numChannels;
        momentaryBlockLoudness.push_back(avgSquare);

        // Maintain the integration window size
        while (momentaryBlockLoudness.size() > (size_t)momentarySamples)
            momentaryBlockLoudness.pop_front();
    }

    // Calculate momentary loudness (400ms window)
    double momentarySum = 0.0;
    for (const auto& s : momentaryBlockLoudness) momentarySum += s;

    if (!momentaryBlockLoudness.empty() && momentarySum > 0.0)
    {
        // Calculate the mean square over the entire window
        float meanSquare = (float)(momentarySum / (double)momentaryBlockLoudness.size());
        // BS.1770 standard calculation (ensure log10 input is positive)
        currentMomentaryLoudness = -0.691f + 10.0f * std::log10(meanSquare + 1e-10f);
    }
    else
    {
        // Report silence if the buffer is empty or contains only silence.
        currentMomentaryLoudness = -144.0f;
    }
}

float LUFSMeter::getMomentaryLoudness() const { return currentMomentaryLoudness; }