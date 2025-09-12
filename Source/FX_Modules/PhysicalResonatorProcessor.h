//================================================================================
// File: FX_Modules/PhysicalResonatorProcessor.h
//================================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <algorithm>

// Include necessary DSP helpers.
#include "../DSPUtils.h"
#include "../DSP_Helpers/TransientDetector.h"
#include "../DSP_Helpers/SpectralAnalyzer.h"

// =============================================================================
// Helper Class: ExcitationGenerator (Blueprint 3.2)
// =============================================================================
class ExcitationGenerator
{
public:
    ExcitationGenerator();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // NEW: Structure to hold parameters
    struct ExcitationParams
    {
        float exciteType = 0.5f;
        float sensitivity = 0.5f;
        int noiseType = 0; // 0: White, 1: Pink
        float attack = 0.001f;
        float decay = 0.05f;
        float sustain = 0.0f;
        float release = 0.01f;
    };

    // Updated process signature
    void process(const juce::dsp::AudioBlock<float>& inputBlock, juce::dsp::AudioBlock<float>& outputExcitationBlock, const ExcitationParams& params);
private:
    double sampleRate = 44100.0;
    // Analysis components
    TransientDetector transientDetector;
    SpectralAnalyzer spectralAnalyzer;

    // Synthesis components
    DSPUtils::NoiseGenerator noiseGen;

    // Use a valid JUCE filter (StateVariableTPTFilter) for color shaping.
    juce::dsp::StateVariableTPTFilter<float> colorFilter;

    // Envelope for impulsive bursts
    juce::ADSR burstEnvelope;

    // FIX: Add declarations for the smoothed values used internally by the generator.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedExciteType;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedSensitivity;
};

// =============================================================================
// Resonator Cores (Abstract Base and Implementations) (Blueprint 3.1)
// =============================================================================

class ResonatorCore
{
public:
    virtual ~ResonatorCore() = default;
    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    // Parameters are passed during process() to allow for per-sample updates.
    virtual void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float tuneHz, float structure, float brightness, float damping, float position) = 0;
};

// -----------------------------------------------------------------------------
// Core I: ModalResonator (Blueprint 3.3)
// -----------------------------------------------------------------------------

class ModalResonator : public ResonatorCore
{
public:
    static constexpr int NUM_MODES = 16;
    // Structure to hold material data (Blueprint Table 3.3.1)
    struct MaterialData {
        std::array<float, NUM_MODES> ratios;
        std::array<float, NUM_MODES> gains;
        std::array<float, NUM_MODES> qs;
    };

    // Static material definitions (Initialized in .cpp)
    static const MaterialData woodData;
    static const MaterialData metalData;
    static const MaterialData inharmonicData;

    // FIX (LNK2001): Declare the methods here. Implementation is moved to the .cpp file.
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float tuneHz, float structure, float brightness, float damping, float position) override;
private:
    void updateModes(float tuneHz, float structure, float brightness, float damping, float position);

    // Define the filter banks.
    // Using a vector of arrays for multi-channel support.
    using FilterType = juce::dsp::StateVariableTPTFilter<float>;
    // Vector (Channels) of Array (Modes)
    std::vector<std::array<FilterType, NUM_MODES>> channelFilterBanks;

    double sampleRate = 44100.0;
    // Storage for calculated mode parameters (needed for Q-normalization)
    std::array<float, NUM_MODES> currentModeGains;
    std::array<float, NUM_MODES> currentModeQs;
};

// -----------------------------------------------------------------------------
// Core II & III: Placeholders (Sympathetic/Inharmonic)
// -----------------------------------------------------------------------------
// Placeholder implementations to allow the architecture to compile.
class SympatheticStringResonator : public ResonatorCore
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override { juce::ignoreUnused(spec); }
    void reset() override {}
    void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float pTuneHz, float pStructure, float pBrightness, float pDamping, float pPosition) override
    {
        // FIX: Silence C4100 (unreferenced parameter) warnings.
        juce::ignoreUnused(excitationBlock, pTuneHz, pStructure, pBrightness, pDamping, pPosition);

        // Placeholder implementation: Clear output.
        outputBlock.clear();
    }
};

class StringResonator : public ResonatorCore
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override { juce::ignoreUnused(spec); }
    void reset() override {}
    void process(const juce::dsp::AudioBlock<float>& excitationBlock, juce::dsp::AudioBlock<float>& outputBlock,
        float pTuneHz, float pStructure, float pBrightness, float pDamping, float pPosition) override
    {
        // FIX: Silence C4100 (unreferenced parameter) warnings.
        juce::ignoreUnused(excitationBlock, pTuneHz, pStructure, pBrightness, pDamping, pPosition);

        // Placeholder implementation: Clear output.
        outputBlock.clear();
    }
};

// =============================================================================
// Main Processor Class
// =============================================================================

class PhysicalResonatorProcessor : public juce::AudioProcessor
{
public:
    PhysicalResonatorProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~PhysicalResonatorProcessor() override = default;

    const juce::String getName() const override { return "Physical Resonator"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Standard JUCE Boilerplate
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

    // --- Components ---
    ExcitationGenerator excitationGenerator;

    // Store concrete instances and use a pointer to the active one.
    ModalResonator modalResonator;
    SympatheticStringResonator sympatheticResonator;
    StringResonator stringResonator;
    ResonatorCore* activeResonator = nullptr;

    // Buffers (Ensure separate buffers for excitation and wet output)
    juce::AudioBuffer<float> excitationBuffer;
    juce::AudioBuffer<float> wetOutputBuffer;

    // --- Parameters and Smoothing ---
    juce::AudioProcessorValueTreeState& mainApvts;
    // Declare all parameter ID strings used in the .cpp
    juce::String modelParamId, tuneParamId, structureParamId, brightnessParamId, dampingParamId, positionParamId, exciteTypeParamId, sensitivityParamId, mixParamId;
    // NEW: Advanced Excitation controls
    juce::String noiseTypeParamId, attackParamId, decayParamId, sustainParamId, releaseParamId;

    // Declare all smoothed values used in the .cpp
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTune;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedStructure;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedBrightness;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDamping;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPosition;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedExciteType;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedSensitivity;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;

    int currentModelIndex = -1;
};