//================================================================================
// File: FX_Modules/PhysicalResonatorProcessor.h (REVISED)
//================================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Assuming these utility classes exist in the project structure
#include "../DSPUtils.h"
#include "../DSP_Helpers/TransientDetector.h"

// ===================== InternalExciter =====================
// Generates a discrete, percussive burst of filtered noise when triggered.
class InternalExciter
{
public:
    InternalExciter();
    void prepare(const juce::dsp::ProcessSpec&);
    void reset();
    void trigger();
    void process(juce::dsp::AudioBlock<float>& outputBlock, float brightness, int noiseType);

private:
    double sampleRate = 44100.0;
    DSPUtils::NoiseGenerator noiseGen;
    juce::dsp::StateVariableTPTFilter<float> colorFilter;
    juce::ADSR envelope;
    juce::ADSR::Parameters envParams;
};

// ===================== ExcitationManager =====================
// Decides whether to use external audio or the internal exciter based on input activity.
class ExcitationManager
{
public:
    void prepare(const juce::dsp::ProcessSpec&);
    void reset();
    void process(const juce::dsp::AudioBlock<float>& inputBlock,
        juce::dsp::AudioBlock<float>& outputExcitationBlock,
        float brightness, float sensitivity, int noiseType);

private:
    InternalExciter internalExciter;
    TransientDetector transientDetector;
    juce::dsp::BallisticsFilter<float> rmsDetector;
    static constexpr float kInputThreshold = 0.01f;
};

// ===================== ResonatorCore (Abstract Base Class) =====================
class ResonatorCore
{
public:
    virtual ~ResonatorCore() = default;
    virtual void prepare(const juce::dsp::ProcessSpec&) = 0;
    virtual void reset() = 0;
    virtual void process(const juce::dsp::AudioBlock<float>& excitationBlock,
        juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) = 0;

protected:
    double sampleRate = 44100.0;
    // Logarithmic mapping from 30 Hz to 8000 Hz
    float tuneToHz(float tuneNorm)
    {
        return 30.0f * std::pow(2.0f, tuneNorm * 8.04f); // ~8 octaves
    }
};

// ===================== ModalResonator (Model 0) =====================
class ModalResonator : public ResonatorCore
{
public:
    // Increased to 60 partials for authentic spectral density.
    static constexpr int NUM_MODES = 60;

    struct MaterialData
    {
        std::array<float, NUM_MODES> ratios;
        std::array<float, NUM_MODES> gains;
        std::array<float, NUM_MODES> qs;
    };

    void prepare(const juce::dsp::ProcessSpec&) override;
    void reset() override;
    void process(const juce::dsp::AudioBlock<float>& excitationBlock,
        juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) override;

private:
    void initializeMaterialTables();
    void computeModeParams(float tuneHz, float structure, float brightness, float damping, float position);

    using Filter = juce::dsp::IIR::Filter<float>;
    std::vector<std::array<Filter, NUM_MODES>> channelFilters;
    std::array<float, NUM_MODES> modeFreqs{};
    std::array<float, NUM_MODES> modeGains{};
    std::array<float, NUM_MODES> modeQs{};

    bool tablesInitialized = false;
    MaterialData woodData{}, metalData{}, glassData{};
};

// ===================== SympatheticStringResonator (Model 1) =====================
class SympatheticStringResonator : public ResonatorCore
{
public:
    static constexpr int NUM_STRINGS = 6;
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::AudioBlock<float>& excitationBlock,
        juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) override;

private:
    void updateTunings(float structure);

    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;
    using LPFilter = juce::dsp::FirstOrderTPTFilter<float>;

    std::vector<std::array<Delay, NUM_STRINGS>> channelDelays;
    std::vector<std::array<LPFilter, NUM_STRINGS>> channelFilters;
    std::vector<std::array<float, NUM_STRINGS>> feedbackGains;
    std::array<float, NUM_STRINGS> currentRatios;

    // Stores the summed feedback state for coupling across samples and blocks
    std::vector<float> summedFeedbackState;

    int maxDelaySamples = 0;
};

// ===================== StringResonator (Extended Karplus-Strong) (Model 2) =====================
class StringResonator : public ResonatorCore
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::AudioBlock<float>& excitationBlock,
        juce::dsp::AudioBlock<float>& outputBlock,
        float tune, float structure, float brightness, float damping, float position) override;

private:
    // Using Lagrange3rd for better quality in Karplus-Strong
    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;
    using DampingFilter = juce::dsp::FirstOrderTPTFilter<float>;
    using DispersionFilter = juce::dsp::IIR::Filter<float>;

    std::vector<Delay> channelDelays;
    std::vector<DampingFilter> channelDampingFilters;
    // Chain of two all-pass filters for rich dispersion.
    std::vector<DispersionFilter> channelDispersionFilters1;
    std::vector<DispersionFilter> channelDispersionFilters2;
    std::vector<float> feedback;
    int maxDelaySamples = 0;
};

// ===================== PhysicalResonatorProcessor (Main Processor) =====================
class PhysicalResonatorProcessor : public juce::AudioProcessor
{
public:
    PhysicalResonatorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex);
    ~PhysicalResonatorProcessor() override = default;

    const juce::String getName() const override { return "Physical Resonator"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    // Boilerplate methods...
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; } // Increased for long decays
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

    ExcitationManager excitationManager;
    ModalResonator modalResonator;
    SympatheticStringResonator sympatheticResonator;
    StringResonator stringResonator;
    ResonatorCore* activeResonator = nullptr;

    juce::AudioBuffer<float> excitationBuffer, wetOutputBuffer;

    juce::AudioProcessorValueTreeState& mainApvts;
    juce::String modelParamId, tuneParamId, structureParamId, brightnessParamId, dampingParamId, positionParamId;
    juce::String sensitivityParamId, mixParamId, noiseTypeParamId;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTune, smoothedStructure, smoothedBrightness, smoothedDamping, smoothedPosition, smoothedMix;

    int currentModelIndex = -1;
    bool instabilityFlag = false;
    juce::dsp::Limiter<float> safetyLimiter;
};