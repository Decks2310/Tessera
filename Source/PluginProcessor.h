//================================================================================
// File: PluginProcessor.h
//================================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SmartAutoGain.h"
#include "Presets/PresetManager.h"

#if JucePlugin_Build_VST3
#define JucePlugin_Vst3Category "Fx"
#endif

// Defines the oversampling quality algorithms
enum class OversamplingAlgorithm
{
    Live,    // Polyphase IIR (Fast, Non-Linear Phase)
    HQ,      // FIR Equiripple (Standard, Linear Phase)
    Deluxe   // FIR Equiripple (High Quality, Linear Phase)
};

// Defines the oversampling rates
enum class OversamplingRate
{
    x1,  // Off
    x2,
    x4,
    x8,
    x16
};

// A wrapper to hold a processing graph and its associated oversampler and buffers.
// This is crucial for seamless, cross-faded graph updates.
struct ProcessingContextWrapper {
    std::unique_ptr<juce::AudioProcessorGraph> graph = std::make_unique<juce::AudioProcessorGraph>();
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> oversampledGraphBuffer; // Dedicated buffer for this context
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

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void parameterChanged(const juce::String&, float) override;

    bool isOversamplingLocked() const { return oversamplingLockActive.load(); }

    std::unique_ptr<PresetManager> presetManager;
    PresetManager* getPresetManager() noexcept { return presetManager.get(); }

    static constexpr int maxSlots = 16;

    int getVisibleSlotCount() const noexcept { return visibleSlotCountInt; }
    void setVisibleSlotCount(int newCount);

    juce::ChangeBroadcaster editorResizeBroadcaster;
    juce::ChangeBroadcaster osLockChangeBroadcaster;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:
    // Authoritative state for visible slots
    int visibleSlotCountInt = 8;

    double preparedSampleRate = 0.0;
    int preparedMaxBlockSize = 0;
    std::atomic<int> currentOSChannels{ 0 };

    // Atomics for thread-safe handling of oversampling configuration
    std::atomic<OversamplingAlgorithm> pendingOSAlgo;
    std::atomic<OversamplingRate> pendingOSRate;
    std::atomic<OversamplingAlgorithm> effectiveOSAlgo;
    std::atomic<OversamplingRate> effectiveOSRate;
    std::atomic<bool> oversamplingLockActive{ false };

    SmartAutoGain smartAutoGain;
    juce::dsp::Gain<float> inputGainStage, outputGainStage;

    // Dual graph system for seamless transitions
    std::unique_ptr<ProcessingContextWrapper> activeContext;
    std::unique_ptr<ProcessingContextWrapper> previousContext;

    // Crossfade management
    enum class FadeState { Idle, Fading };
    std::atomic<FadeState> fadeState{ FadeState::Idle };
    juce::AudioBuffer<float> fadeBuffer;
    int fadeSamplesRemaining = 0;
    int totalFadeSamples = 0;
    static constexpr double crossfadeDurationMs = 10.0;

    // Graph node management
    juce::AudioProcessorGraph::Node::Ptr inputNode, outputNode;
    std::vector<juce::AudioProcessorGraph::Node::Ptr> fxSlotNodes;
    std::atomic<bool> isGraphDirty{ true };

    juce::AudioBuffer<float> dryBufferForMixing;

    // Private Helper Methods
    bool updateGraph();
    void initiateGraphUpdate();
    void updateOversamplingConfiguration();
    bool checkForChromaTapeUsage() const;
    std::unique_ptr<juce::AudioProcessor> createProcessorForChoice(int choice, int slotIndex);
    void updateSmartAutoGainParameters();
    void updateGainStages();
    std::unique_ptr<juce::dsp::Oversampling<float>> createOversamplingEngine(OversamplingRate, OversamplingAlgorithm, int numChannels);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModularMultiFxAudioProcessor)
};