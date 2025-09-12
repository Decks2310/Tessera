//================================================================================
// File: FX_Modules/SpectralAnimatorEngine.h
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>

class SpectralAnimatorEngine
{
public:
    static constexpr int FFT_ORDER = 11;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int HOP_SIZE = FFT_SIZE / 4;
    static constexpr int NUM_BINS = FFT_SIZE / 2 + 1;

    enum class Mode { Pitch, Formant };

    SpectralAnimatorEngine();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setMode(Mode newMode);
    void setPitch(float newPitchHz);
    void setFormant(float x, float y);
    void setMorph(float amount);
    void setTransientPreservation(float amount);
private:
    void processFrame(int channel);
    void updateMasks();

    struct FormantProfile { float f1, f2; };
    FormantProfile getVowel(float x, float y);

    double sampleRate = 44100.0;
    int numChannels = 0;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    juce::AudioBuffer<float> inputFIFO;
    juce::AudioBuffer<float> outputBuffer;
    int fifoIndex = 0;
    int outputBufferWritePos = 0;
    int outputBufferReadPos = 0;

    std::vector<std::vector<float>> channelTimeDomain;
    std::vector<std::vector<float>> channelFreqDomain;

    struct TransientDetectorChannel {
        juce::dsp::FirstOrderTPTFilter<float> highPassFilter;
        juce::dsp::BallisticsFilter<float> envelopeFollower;
        float transientMix = 0.0f;
        float decayFactor = 0.99f;
    };
    std::vector<TransientDetectorChannel> transientDetectors;
    const float transientThreshold = 0.05f;

    // --- Parameters ---
    Mode currentMode = Mode::Pitch;
    float pitchHz = 440.0f;

    // ✅ FIX: Replaced juce::Point<float> with a simple internal struct
    struct XYPair { float x = 0.5f; float y = 0.5f; };
    XYPair formantXY;

    // FIX: Replaced raw floats with SmoothedValue for glitch-free modulation.
    // float morphAmount = 1.0f; // REMOVED
    // float transientPreservation = 1.0f; // REMOVED
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMorph;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTransientPreservation;


    // --- Spectral Masks (Shared across channels) ---
    std::vector<float> harmonicMask;
    std::vector<float> formantMask;
    bool masksNeedUpdate = true;
};
