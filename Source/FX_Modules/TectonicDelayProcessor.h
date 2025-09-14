//==============================================================================
/*
    TectonicDelayProcessor.h (Simplified TubeEngine)
*/
//==============================================================================
#pragma once
#include <JuceHeader.h>
#include "../DSPUtils.h"
#include "../DSP_Helpers/InterpolatedCircularBuffer.h"

class TectonicDelayProcessor : public juce::AudioProcessor
{
public:
    TectonicDelayProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex);
    ~TectonicDelayProcessor() override = default;

    const juce::String getName() const override { return "Tectonic Delay"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override; void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {} const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {} void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    struct CrossoverNetwork
    {
        juce::dsp::LinkwitzRileyFilter<float> lowMidLowpass, lowMidHighpass, midHighLowpass, midHighHighpass;
        juce::AudioBuffer<float> lowBand, midBand, highBand;
        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();
        void setCrossoverFrequencies(float lowMid, float midHigh);
        void processBlock(juce::AudioBuffer<float>& buffer);
        juce::AudioBuffer<float>& getLowBand()  { return lowBand; }
        juce::AudioBuffer<float>& getMidBand()  { return midBand; }
        juce::AudioBuffer<float>& getHighBand() { return highBand; }
    };

    struct TubeEngine
    {
        void prepare(double sr, int channels, int /*maxBlock*/) { sampleRate = sr; numCh = channels; noise.setSeedRandomly(); rng.setSeedRandomly(); }
        void reset() { lastSat = lastOut = pitchFrac = 0.0f; }
        void process(juce::AudioBuffer<float>& buffer, float driveDb, float texture, float density, float pitch)
        {
            int numSamples = buffer.getNumSamples(); int chs = juce::jmin(buffer.getNumChannels(), numCh);
            float drive = juce::Decibels::decibelsToGain(driveDb);
            texture = juce::jlimit(0.0f, 1.0f, texture); density = juce::jlimit(0.0f, 1.0f, density); pitch = juce::jlimit(-24.0f, 24.0f, pitch);
            float pitchRatio = std::pow(2.0f, pitch / 12.0f); float driftInc = (pitchRatio - 1.0f) * 0.001f;
            for (int ch = 0; ch < chs; ++ch)
            {
                float* d = buffer.getWritePointer(ch); float localLastOut = lastOut;
                for (int i = 0; i < numSamples; ++i)
                {
                    float x = d[i] * drive;
                    float asym = 0.3f + 0.7f * texture; float pos = x >= 0 ? x : 0; float neg = x < 0 ? x : 0;
                    x = pos / (1.0f + asym * pos) + neg / (1.0f - (1.0f - asym) * neg);
                    x = DSPUtils::fastTanh(x * (0.8f + 0.4f * texture));
                    if (density > 0.001f && rng.nextFloat() < density * 0.0025f)
                        x += (rng.nextFloat() * 2.0f - 1.0f) * (0.05f + 0.15f * density);
                    float delta = x - lastSat; float limit = 0.4f + 0.6f * (1.0f - texture); delta = juce::jlimit(-limit, limit, delta); lastSat += delta; x = lastSat;
                    pitchFrac += driftInc; if (pitchFrac > 1.0f) pitchFrac -= 1.0f; x = x * (1.0f - pitchFrac * 0.1f) + localLastOut * (pitchFrac * 0.1f); localLastOut = x; d[i] = x;
                    if (density > 0.0005f) d[i] += (noise.nextFloat() * 2.0f - 1.0f) * 0.01f * density * texture;
                }
                lastOut = localLastOut;
            }
        }
        double sampleRate = 44100.0; int numCh = 2; juce::Random noise, rng; float lastSat = 0.0f, lastOut = 0.0f, pitchFrac = 0.0f;
    };

    struct DelayBand
    {
        void prepare(const juce::dsp::ProcessSpec& spec, double sr, int samplesPerBlock)
        { sampleRate = sr; delayBuffer.prepare(spec, (int)(sampleRate * 4.0)); workingBuffer.setSize((int)spec.numChannels, samplesPerBlock); delayOutput.setSize((int)spec.numChannels, samplesPerBlock); tube.prepare(sr, (int)spec.numChannels, samplesPerBlock); smoothedDelayTime.reset(sr, 0.1); smoothedDelayTime.setCurrentAndTargetValue(100.0f); }
        void reset() { delayBuffer.reset(); workingBuffer.clear(); delayOutput.clear(); tube.reset(); smoothedDelayTime.setCurrentAndTargetValue(100.0f); }
        void processBlock(juce::AudioBuffer<float>& bandInput, float delayTimeMs, float feedback, float drive, float texture, float density, float pitch)
        {
            int numSamples = bandInput.getNumSamples(); int channels = bandInput.getNumChannels(); workingBuffer.setSize(channels, numSamples, false, false, true); delayOutput.setSize(channels, numSamples, false, false, true); workingBuffer.clear(); delayOutput.clear(); smoothedDelayTime.setTargetValue(delayTimeMs);
            for (int i = 0; i < numSamples; ++i)
            { float currentDelayMs = smoothedDelayTime.getNextValue(); float delaySamples = juce::jlimit(1.0f, (float)delayBuffer.getSize() - 2.0f, currentDelayMs * (float)sampleRate * 0.001f); for (int ch = 0; ch < channels; ++ch) { float in = bandInput.getSample(ch, i); float readPos = (float)delayBuffer.getWritePosition() - delaySamples - (float)i; float delayed = delayBuffer.read(ch, readPos); float fbSample = delayed * feedback; delayBuffer.writeSample(ch, in + fbSample); delayOutput.setSample(ch, i, delayed); } delayBuffer.advanceWritePosition(); }
            workingBuffer.makeCopyOf(delayOutput); tube.process(workingBuffer, drive, texture, density, pitch); bandInput.makeCopyOf(workingBuffer);
        }
        InterpolatedCircularBuffer delayBuffer; juce::AudioBuffer<float> workingBuffer, delayOutput; juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedDelayTime; TubeEngine tube; double sampleRate = 44100.0;
    };

    void updateParameters();
    CrossoverNetwork crossover; std::array<DelayBand, 3> delayBands; juce::AudioBuffer<float> dryBuffer, wetBuffer; juce::AudioProcessorValueTreeState& mainApvts;
    juce::String lowTimeParamId, midTimeParamId, highTimeParamId, feedbackParamId, lowMidCrossoverParamId, midHighCrossoverParamId, decayDriveParamId, decayTextureParamId, decayDensityParamId, decayPitchParamId, linkParamId, mixParamId;
    struct TectonicParameters { float lowTime = 100.0f, midTime = 200.0f, highTime = 150.0f, feedback = 0.3f, lowMidCrossover = 400.0f, midHighCrossover = 2500.0f, decayDrive = 6.0f, decayTexture = 0.5f, decayDensity = 0.5f, decayPitch = 0.0f; bool linked = true; float mix = 0.5f; } params;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFeedback, smoothedDecayDrive, smoothedDecayTexture, smoothedDecayDensity, smoothedDecayPitch, smoothedMix; double sampleRate = 44100.0; int maxBlockSize = 512;
};