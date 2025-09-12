// File: PluginProcessor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SmartAutoGain.h"

#if JucePlugin_Build_VST3
#define JucePlugin_Vst3Category "Fx"
#endif

class ModularMultiFxAudioProcessorEditor;

// NEW: Define Oversampling Configuration Enums
enum class OversamplingAlgorithm
{
    Live,    // Polyphase IIR (Fast, Non-Linear Phase)
    HQ,      // FIR Equiripple (Standard, Linear Phase)
    Deluxe   // FIR Equiripple (High Quality, Linear Phase)
};

enum class OversamplingRate
{
    x1,  // Off
    x2,
    x4,
    x8,
    x16
};

// FIX Issue 2: Structure to hold a graph, its oversampler, and its dedicated buffer
struct ProcessingContextWrapper {
    std::unique_ptr<juce::AudioProcessorGraph> graph = std::make_unique<juce::AudioProcessorGraph>();
    // UPDATED: Now owns the oversampler instance for this context.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    // Dedicated buffer for this context's oversampled processing.
    juce::AudioBuffer<float> oversampledGraphBuffer;
};

class ModularMultiFxAudioProcessor : public juce::AudioProcessor,
    public juce::AudioProcessorValueTreeState::Listener
{
public:
    ModularMultiFxAudioProcessor();
    ~ModularMultiFxAudioProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // ... (Boilerplate methods omitted for brevity, remain the same as original) ...
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName(int index, const juce::String& newName) override { juce::ignoreUnused(index, newName); }
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // NEW: Public method for the Editor to check the lock status
    bool isOversamplingLocked() const { return oversamplingLockActive.load(); }

    // NEW: Broadcaster for OS Lock changes (distinct from editorResizeBroadcaster)
    juce::ChangeBroadcaster osLockChangeBroadcaster;

    static constexpr int maxSlots = 16;
    juce::Value visibleSlotCount;
    juce::ChangeBroadcaster editorResizeBroadcaster;


    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters", createParameterLayout() };
private:
    // ✅ FIX G: Store maximum block size and sample rate for reliable graph configuration
    double preparedSampleRate = 0.0;
    int preparedMaxBlockSize = 0;

    bool updateGraph();
    void initiateGraphUpdate();
    // NEW: Helper functions for centralized OS management
    void updateOversamplingConfiguration();
    bool checkForChromaTapeUsage() const;
    std::unique_ptr<juce::AudioProcessor> createProcessorForChoice(int choice, int slotIndex);
    void updateSmartAutoGainParameters();
    void updateGainStages();

    // UPDATED: Helper to create a specific engine instance dynamically.
    std::unique_ptr<juce::dsp::Oversampling<float>> createOversamplingEngine(OversamplingRate rate, OversamplingAlgorithm algo, int numChannels);

    // REMOVED: setupOversamplingEngines() and osEngines map.

    // UPDATED: Atomics to track the desired configuration (User's UI selection).
    std::atomic<OversamplingAlgorithm> pendingOSAlgo;
    std::atomic<OversamplingRate> pendingOSRate;

    // NEW: Atomics to track the effective configuration (After applying locks/overrides).
    std::atomic<OversamplingAlgorithm> effectiveOSAlgo;
    std::atomic<OversamplingRate> effectiveOSRate;

    // NEW: Atomic to track the lock status
    std::atomic<bool> oversamplingLockActive{ false };

    // ✅ FIX D: Track the number of channels currently configured (used for dynamic reconfiguration detection)
    std::atomic<int> currentOSChannels{ 0 };
    SmartAutoGain smartAutoGain;

    // NEW: Input and Output Gain Stages for integrated gain staging
    juce::dsp::Gain<float> inputGainStage;
    juce::dsp::Gain<float> outputGainStage;
    // === FIX N: Delay line for Dry Path Latency Compensation ===
    // REMOVED: No longer needed as the new AutoGain is zero-latency.
    // ===========================================================

    // ✅ UPDATED: Dual Graph System for Crossfading (Now using Context Wrapper)
    std::unique_ptr<ProcessingContextWrapper> activeContext;
    // Holds the previous context during crossfade
    std::unique_ptr<ProcessingContextWrapper> previousContext;
    // ✅ NEW: Crossfade Management
    enum class FadeState { Idle, Fading };
    std::atomic<FadeState> fadeState{ FadeState::Idle };
    juce::AudioBuffer<float> fadeBuffer; // Buffer for the previous graph output
    int fadeSamplesRemaining = 0;
    int totalFadeSamples = 0;
    static constexpr double crossfadeDurationMs = 10.0; // 10ms crossfade


    // These pointers are now relative to the *activeContext->graph* during updateGraph()
    juce::AudioProcessorGraph::Node::Ptr inputNode;
    juce::AudioProcessorGraph::Node::Ptr outputNode;
    std::vector<juce::AudioProcessorGraph::Node::Ptr> fxSlotNodes;
    std::atomic<bool> isGraphDirty{ true };

    juce::AudioBuffer<float> dryBufferForMixing;
    // ✅ FIX E: Removed global oversampledGraphBuffer, now managed within ProcessingContextWrapper.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModularMultiFxAudioProcessor)
};