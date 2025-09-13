//================================================================================
// File: FX_Modules/ChromaTapeProcessor.h (REVISED)
//================================================================================
#pragma once
#include <JuceHeader.h>
#include "../DSPUtils.h"
// CHANGED: Include the optimized saturation model
#include "TapeSaturation.h"

class ChromaTapeProcessor : public juce::AudioProcessor
{
public:
    ChromaTapeProcessor(juce::AudioProcessorValueTreeState& mainApvts, int slotIndex);
    ~ChromaTapeProcessor() override;

    const juce::String getName() const override { return "ChromaTape"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Boilerplate methods (unchanged)
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    static constexpr int NUM_BANDS = 3;
    enum BandIndex { LOW = 0, MID = 1, HIGH = 2 };

    //==============================================================================
    // Revised TapeBand Structure (Optimized)
    //==============================================================================
    struct TapeBand
    {
        // --- Saturation & Core ---
        TapeDSP::OptimizedTapeSaturator saturator;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedSaturationDb;

        // NEW (Blueprint II.3): State for Mid Band hysteresis model (per channel)
        std::vector<float> hysteresis_last_input;

        // NEW (Blueprint II.2, II.4): EQ Stages

        // Low Band: IIR Peak Filter for Head Bump. We need per-channel instances.
        using EQFilter = juce::dsp::IIR::Filter<float>;
        std::vector<EQFilter> headBumpFilters;

        // High Band: TPT LPF for Dynamic HF Loss.
        juce::dsp::StateVariableTPTFilter<float> dynamicHfFilter;
        DSPUtils::EnvelopeFollower hfEnvelope;

        // --- Mechanical Degradation Stage (Wow & Flutter) ---
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
        DSPUtils::LFO wowLFO;
        DSPUtils::LFO flutterLFO;
        DSPUtils::NoiseGenerator noiseGen;
        juce::dsp::FirstOrderTPTFilter<float> noiseFilter; // Mono control signal filter

        // OPTIMIZATION FIX: Modulation smoother must be stereo.
        std::vector<juce::dsp::FirstOrderTPTFilter<float>> modSmoothers;

        // Scrape Flutter (Mono control signal filter)
        juce::dsp::StateVariableTPTFilter<float> scrapeNoiseFilter;

        // NEW (Blueprint III.3): Chaotic Modulator
        float chaos_state{ 0.5f };
        static constexpr float CHAOS_R = 3.9f; // Logistic map parameter for chaos

        // Parameter smoothing (smoothedWowDepth, smoothedFlutterDepth remain)
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedWowDepth;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFlutterDepth;

        // Storage for stereo modulation values (LFOs)
        std::pair<float, float> currentWow = { 0.0f, 0.0f };
        std::pair<float, float> currentFlutter = { 0.0f, 0.0f };

        // OPTIMIZATION FIX: Storage for noise components (advanced once per frame)
        float currentFilteredNoise = 0.0f;
        float currentScrapeNoise = 0.0f;
    };

    std::array<TapeBand, NUM_BANDS> bands;

    // Crossover Network (Structure unchanged)
    struct CrossoverNetwork
    {
        juce::dsp::LinkwitzRileyFilter<float> lowMidLowpass;
        juce::dsp::LinkwitzRileyFilter<float> lowMidHighpass;
        juce::dsp::LinkwitzRileyFilter<float> midHighLowpass;
        juce::dsp::LinkwitzRileyFilter<float> midHighHighpass;

        juce::AudioBuffer<float> lowBand, midBand, highBand;

        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();
        void setCrossoverFrequencies(float lowMid, float midHigh);
        void processBlock(juce::AudioBuffer<float>& buffer);

        juce::AudioBuffer<float>& getLowBand() { return lowBand; }
        juce::AudioBuffer<float>& getMidBand() { return midBand; }
        juce::AudioBuffer<float>& getHighBand() { return highBand; }
    };

    CrossoverNetwork crossover;

    // NEW (Blueprint IV): Noise and Hum Generators
    DSPUtils::NoiseGenerator hissGenerator;
    // Hiss shaping (High Shelf). Need per-channel instances.
    std::vector<juce::dsp::IIR::Filter<float>> hissShapingFilters;

    DSPUtils::LFO humOscillator;
    DSPUtils::LFO humHarmonicOscillator;

    // OPTIMIZATION FIX: Global Parameter Smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedScrape;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedChaos;
    // Use Multiplicative smoothing for gain levels (Hiss/Hum)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedHissLevel;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedHumLevel;

    juce::AudioProcessorValueTreeState& mainApvts;
    std::array<juce::String, NUM_BANDS> saturationParamIds;
    std::array<juce::String, NUM_BANDS> wowParamIds;
    std::array<juce::String, NUM_BANDS> flutterParamIds;
    juce::String lowMidCrossoverParamId, midHighCrossoverParamId;

    // NEW: Parameter IDs
    juce::String scrapeParamId, chaosParamId, hissParamId, humParamId;
    juce::String headBumpFreqParamId, headBumpGainParamId;

    // NEW: Helper methods for the refactored processing
    void updateParameters();
    void processBand(int bandIdx, int sample, int numChannels, juce::AudioBuffer<float>& buffer);
    void updateModulation(int bandIdx);
    float applyModulation(int bandIdx, int channel, float inputSample);
};