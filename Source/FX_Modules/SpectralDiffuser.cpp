//================================================================================
// File: FX_Modules/SpectralDiffuser.cpp
//================================================================================
#include "SpectralDiffuser.h"

namespace { static std::vector<std::vector<float>> accumulatedPhase; }

SpectralDiffuser::SpectralDiffuser()
    : fft(FFT_ORDER),
      window(FFT_SIZE, juce::dsp::WindowingFunction<float>::hann, false),
      distribution(-juce::MathConstants<float>::pi, juce::MathConstants<float>::pi)
{
    randomEngine.seed((unsigned)juce::Time::getMillisecondCounter());
}

void SpectralDiffuser::prepare(const juce::dsp::ProcessSpec& spec)
{
    int numChannels = (int)spec.numChannels;
    inputFIFO.setSize(numChannels, FFT_SIZE);
    outputFIFO.setSize(numChannels, FFT_SIZE);
    fftData.resize(numChannels);
    for (auto& c : fftData) c.assign(FFT_SIZE * 2, 0.0f);
    accumulatedPhase.assign(numChannels, std::vector<float>(FFT_SIZE / 2, 0.0f));
    prevDiffusion = 0.0f;
    fifoIndex = 0;
    inputFIFO.clear();
    outputFIFO.clear();
}

void SpectralDiffuser::reset()
{
    fifoIndex = 0;
    inputFIFO.clear();
    outputFIFO.clear();
    for (auto& c : accumulatedPhase) std::fill(c.begin(), c.end(), 0.0f);
    prevDiffusion = 0.0f;
}

void SpectralDiffuser::process(juce::AudioBuffer<float>& buffer, float diffusionAmount)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float target = diffusionAmount;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float in = buffer.getSample(ch, i);
            inputFIFO.setSample(ch, fifoIndex, in);
            float outSample = outputFIFO.getSample(ch, fifoIndex);
            buffer.setSample(ch, i, outSample);
            outputFIFO.setSample(ch, fifoIndex, 0.0f);
        }
        ++fifoIndex;
        if (fifoIndex == FFT_SIZE)
        {
            prevDiffusion = 0.85f * prevDiffusion + 0.15f * target;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                processFrame(ch, prevDiffusion);
                for (int j = 0; j < HOP_SIZE; ++j)
                    inputFIFO.setSample(ch, j, inputFIFO.getSample(ch, j + HOP_SIZE));
            }
            fifoIndex = HOP_SIZE;
        }
    }
}

void SpectralDiffuser::processFrame(int channel, float diffusionAmount)
{
    auto& data = fftData[channel];
    std::copy(inputFIFO.getReadPointer(channel),
              inputFIFO.getReadPointer(channel) + FFT_SIZE,
              data.begin());

    window.multiplyWithWindowingTable(data.data(), FFT_SIZE);
    fft.performRealOnlyForwardTransform(data.data());

    double preEnergy = 0.0;
    if (normalizeOutput)
    {
        for (int i = 0; i < FFT_SIZE; ++i)
        {
            float r = data[2 * i];
            preEnergy += r * r;
            if (2 * i + 1 < (int)data.size())
            {
                float im = data[2 * i + 1];
                preEnergy += im * im;
            }
        }
    }

    for (int bin = 1; bin < FFT_SIZE / 2; ++bin)
    {
        float real = data[2 * bin];
        float imag = data[2 * bin + 1];
        float mag  = std::sqrt(real * real + imag * imag);
        float phase = std::atan2(imag, real);
        float delta = distribution(randomEngine) * diffusionAmount * 0.15f * phaseDriftScale;
        accumulatedPhase[channel][bin] += delta;
        if (accumulatedPhase[channel][bin] > juce::MathConstants<float>::pi)
            accumulatedPhase[channel][bin] -= juce::MathConstants<float>::twoPi;
        else if (accumulatedPhase[channel][bin] < -juce::MathConstants<float>::pi)
            accumulatedPhase[channel][bin] += juce::MathConstants<float>::twoPi;
        float newPhase = phase + accumulatedPhase[channel][bin];
        data[2 * bin]     = mag * std::cos(newPhase);
        data[2 * bin + 1] = mag * std::sin(newPhase);
    }

    fft.performRealOnlyInverseTransform(data.data());
    window.multiplyWithWindowingTable(data.data(), FFT_SIZE);

    if (normalizeOutput)
    {
        double postEnergy = 0.0;
        for (int i = 0; i < FFT_SIZE; ++i)
        {
            float s = data[i];
            postEnergy += s * s;
        }
        if (postEnergy > 1e-12 && preEnergy > 1e-12)
        {
            float g = (float)std::sqrt(preEnergy / postEnergy);
            for (int i = 0; i < FFT_SIZE; ++i) data[i] *= g;
        }
    }

    for (int i = 0; i < FFT_SIZE; ++i)
        outputFIFO.addSample(channel, i, data[i]);
}