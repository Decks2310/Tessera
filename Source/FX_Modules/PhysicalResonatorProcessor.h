//================================================================================
// File: FX_Modules/PhysicalResonatorProcessor.h
//================================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
// FIX: Removed the invalid internal include. All public DSP classes are included via juce_dsp.h.
// #include <juce_dsp/processors/juce_AllpassTPTFilter.h>

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Assuming these includes exist in your project structure
#include "../DSPUtils.h"
#include "../DSP_Helpers/TransientDetector.h"
#include "../DSP_Helpers/SpectralAnalyzer.h"

// ===================== ExcitationGenerator =====================
class ExcitationGenerator {
public:
    ExcitationGenerator();
    void prepare(const juce::dsp::ProcessSpec&);
    void reset();
    struct ExcitationParams {
        float exciteType = 0.5f;
        float sensitivity = 0.5f;
        int noiseType = 0;
        // ADSR parameters are now actively used for the impulsive excitation.
        float attack = 0.001f; float decay = 0.05f; float sustain = 0.0f; float release = 0.01f;
    };
    void process(const juce::dsp::AudioBlock<float>& inputBlock, juce::dsp::AudioBlock<float>& outputExcitationBlock, const ExcitationParams& params);
private:
    double sampleRate = 44100.0;
    TransientDetector transientDetector;
    SpectralAnalyzer spectralAnalyzer;
    DSPUtils::NoiseGenerator noiseGen;
    juce::dsp::StateVariableTPTFilter<float> colorFilter;
    juce::ADSR burstEnvelope;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedExciteType, smoothedSensitivity;
};

// ===================== ResonatorCore (Abstract Base Class) =====================
class ResonatorCore {
public:
    virtual ~ResonatorCore() = default;
    virtual void prepare(const juce::dsp::ProcessSpec&) = 0;
    virtual void reset() = 0;
    // Parameters are passed normalized (0.0 - 1.0)
    virtual void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) = 0;
protected:
    double sampleRate = 44100.0;
    // Helper to convert normalized Tune (0-1) to Hz
    float tuneToHz(float tuneNorm) {
        // Logarithmic mapping from 50 Hz to 4000 Hz
        return 50.0f * std::pow(2.0f, tuneNorm * 6.3219f); // 6.3219 octaves
    }
};

// ===================== ModalResonator (Model 0) =====================
class ModalResonator : public ResonatorCore {
public:
    static constexpr int NUM_MODES = 24;
    struct MaterialData {
        std::array<float, NUM_MODES> ratios;
        std::array<float, NUM_MODES> gains;
        std::array<float, NUM_MODES> qs;
    };
    void prepare(const juce::dsp::ProcessSpec&) override;
    void reset() override;
    void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) override;
private:
    void initializeMaterialTables();
    void computeModeParams(float tuneHz, float structure, float brightness, float damping, float position);

    // FIX: Switched from IIR::Filter to StateVariableTPTFilter for massive CPU optimization.
    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    // Structure: [Channel][Mode]
    std::vector<std::array<Filter, NUM_MODES>> channelFilters;

    std::array<float, NUM_MODES> modeFreqs{};
    std::array<float, NUM_MODES> modeGains{};
    std::array<float, NUM_MODES> modeQs{};

    bool tablesInitialized = false;
    MaterialData woodData{}, metalData{}, glassData{};
};

// ===================== SympatheticStringResonator (Model 1) =====================
class SympatheticStringResonator : public ResonatorCore {
public:
    static constexpr int NUM_STRINGS = 6;
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) override;

private:
    void updateTunings(float structure);

    // FIX: Use higher-order interpolation (Lagrange3rd) to reduce artifacts when modulating delay time.
    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;
    // Structure: [Channel][String]
    std::vector<std::array<Delay, NUM_STRINGS>> channelDelays;

    // Damping filter in the feedback loop
    using LPFilter = juce::dsp::FirstOrderTPTFilter<float>;
    std::vector<std::array<LPFilter, NUM_STRINGS>> channelFilters;

    // FIX: Added smoothed delay times to prevent zipper noise when modulating Tune/Structure.
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, NUM_STRINGS> smoothedDelayTimes;

    std::array<float, NUM_STRINGS> currentRatios;
    int maxDelaySamples = 0;
    // Storing feedback explicitly for the comb filter implementation
    std::vector<std::array<float, NUM_STRINGS>> feedbackGains;
};

// ===================== StringResonator (Karplus-Strong) (Model 2) =====================
class StringResonator : public ResonatorCore {
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) override;
private:
    // Main delay line
    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;
    std::vector<Delay> channelDelays;

    // FIX: Added smoothed delay time to prevent zipper noise.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayTime;

    // Damping filter (Brightness)
    using DampingFilter = juce::dsp::FirstOrderTPTFilter<float>;
    std::vector<DampingFilter> channelDampingFilters;

    // Dispersion filter (Inharmonicity/Structure).
    // FIX: Replaced the non-public juce::dsp::AllpassTPTFilter with juce::dsp::IIR::Filter.
    // This allows Q control for the all-pass filter, although it is not TPT.
    using DispersionFilter = juce::dsp::IIR::Filter<float>;
    std::vector<DispersionFilter> channelDispersionFilters;

    int maxDelaySamples = 0;
    std::vector<float> feedback; // Per channel feedback storage
};

// ===================== PhysicalResonatorProcessor (Main Processor) =====================
class PhysicalResonatorProcessor : public juce::AudioProcessor {
public:
    PhysicalResonatorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex);
    ~PhysicalResonatorProcessor() override = default;

    // Standard Processor Methods (Boilerplate omitted for brevity)
    const juce::String getName() const override { return "Physical Resonator"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    void updateResonatorCore(int newModelIndex);
    bool checkAndHandleInstability(float sampleValue);

    // DSP Components
    ExcitationGenerator excitationGenerator;
    ModalResonator modalResonator;
    SympatheticStringResonator sympatheticResonator;
    StringResonator stringResonator;
    ResonatorCore* activeResonator = nullptr;

    // Buffers
    juce::AudioBuffer<float> excitationBuffer, wetOutputBuffer;

    // Parameter Handling
    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String modelParamId, tuneParamId, structureParamId, brightnessParamId, dampingParamId, positionParamId;
    juce::String exciteTypeParamId, sensitivityParamId, mixParamId, noiseTypeParamId;
    juce::String attackParamId, decayParamId, sustainParamId, releaseParamId;

    // Smoothed Parameters (operating in normalized 0-1 range)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTune, smoothedStructure, smoothedBrightness, smoothedDamping, smoothedPosition, smoothedMix;

    int currentModelIndex = -1;
    bool instabilityFlag = false;

    // Safety Limiter
    juce::dsp::Limiter<float> safetyLimiter;
};