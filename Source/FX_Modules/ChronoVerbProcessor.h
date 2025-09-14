#pragma once
#include <memory>
#include <vector>
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../../Source/DSPUtils.h"
#include "../../Source/DSP_Helpers/InterpolatedCircularBuffer.h"
#include "../../Source/FX_Modules/SpectralDiffuser.h"

class ChronoVerbProcessor : public juce::AudioProcessor
{
public:
    ChronoVerbProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex);
    ~ChronoVerbProcessor() override = default;

    const juce::String getName() const override { return "Chrono-Verb Zenith"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

private:
    //==============================================================================
    // Clean tap definition for early reflections
    struct TapDefinition
    {
        float delayRatio;  // Delay time as ratio of size parameter
        float gain;        // Tap amplitude
        float pan;         // Stereo pan position (-1 to 1)
    };

    //==============================================================================
    // Path A: Early Reflections Generator (Multi-tap delay)
    class EarlyReflectionsGenerator
    {
    public:
        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();
        void processBlock(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, 
                         float size, float modulation);
        
    private:
        static constexpr int NUM_TAPS = 8;
        std::array<TapDefinition, NUM_TAPS> tapDefinitions;
        
        InterpolatedCircularBuffer multiTapDelay;
        DSPUtils::LFO modLFO;
        
        double sampleRate = 44100.0;
        int numChannels = 2;
    };

    //==============================================================================
    // Path B: Late Reflections Generator (Spectral diffusion)
    class LateReflectionsGenerator
    {
    public:
        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();
        void processBlock(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, 
                         float diffusion);
        int getLatencySamples() const { return diffuser.getLatencyInSamples(); }

    private:
        SpectralDiffuser diffuser;
        double sampleRate = 44100.0;
        int numChannels = 2;
    };

    //==============================================================================
    // Simple feedback path with damping
    class FeedbackPath
    {
    public:
        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();
        void processBlock(juce::AudioBuffer<float>& buffer, float damping);

    private:
        juce::dsp::StateVariableTPTFilter<float> dampingFilter;
        double sampleRate = 44100.0;
        int numChannels = 2;
    };

    //==============================================================================
    void updateParameters();

    //==============================================================================
    // DSP Modules - Clean architecture
    EarlyReflectionsGenerator earlyReflections;
    LateReflectionsGenerator lateReflections;
    FeedbackPath feedbackPath;
    
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> latencyCompensationDelay;

    // Buffers
    juce::AudioBuffer<float> preDelayBuffer;
    juce::AudioBuffer<float> earlyReflectionsBuffer;
    juce::AudioBuffer<float> lateReflectionsBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> feedbackBuffer;

    // Parameters - Original simple set
    struct ChronoVerbParameters
    {
        float size = 0.5f;        // Pre-delay + ER spacing
        float decay = 0.6f;       // Feedback gain (0-110%)
        float balance = 0.5f;     // ER vs LR mix
        bool freeze = false;      // Infinite sustain
        float diffusion = 0.7f;   // SpectralDiffuser amount
        float damping = 0.5f;     // Feedback filter (200Hz-20kHz)
        float modulation = 0.2f;  // LFO depth on ER taps
        float mix = 0.5f;         // Wet/dry blend
    };
    ChronoVerbParameters params;

    // Smoothed Parameters - Proper smoothing time
    juce::SmoothedValue<float> smSize, smDecay, smBalance, smDiffusion, 
                               smDamping, smModulation, smMix;

    // Parameter IDs
    juce::String sizeParamId, decayParamId, balanceParamId, freezeParamId,
                 diffusionParamId, dampingParamId, modulationParamId, mixParamId;

    // Member variables
    juce::AudioProcessorValueTreeState& mainApvts;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChronoVerbProcessor)
};