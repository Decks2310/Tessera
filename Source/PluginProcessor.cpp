//================================================================================
// File: PluginProcessor.cpp
//================================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FX_Modules/DistortionProcessor.h"
#include "FX_Modules/FilterProcessor.h"
#include "FX_Modules/ModulationProcessor.h"
#include "FX_Modules/AdvancedDelayProcessor.h"
#include "FX_Modules/ReverbProcessor.h"
#include "FX_Modules/AdvancedCompressorProcessor.h"
#include "FX_Modules/ChromaTapeProcessor.h"
// REMOVED: #include "FX_Modules/BBDCloudProcessor.h"
// REMOVED: #include "FX_Modules/FractureTubeProcessor.h"
#include "FX_Modules/MorphoCompProcessor.h"
#include "FX_Modules/PhysicalResonatorProcessor.h"
// NEW INCLUDE
#include "FX_Modules/SpectralAnimatorProcessor.h"

// FIX 1: Corrected PassThroughProcessor Implementation
class PassThroughProcessor : public juce::AudioProcessor
{
public:
    PassThroughProcessor()
        : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void reset() override {}
    // Correct implementation: Clear unused channels, audio passes through implicitly.
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ignoreUnused(midiMessages);
        juce::ScopedNoDenormals noDenormals;
        for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
            buffer.clear(i, 0, buffer.getNumSamples());
    }
    const juce::String getName() const override {
        return "PassThrough";
    }
    juce::AudioProcessorEditor* createEditor() override {
        return nullptr;
    }
    bool hasEditor() const override {
        return false;
    }
    bool acceptsMidi() const override {
        return false;
    }
    bool producesMidi() const override {
        return false;
    }
    double getTailLengthSeconds() const override {
        return 0.0;
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {
        return {};
    }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PassThroughProcessor)
};
// FIX 2: Updated Constructor
ModularMultiFxAudioProcessor::ModularMultiFxAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    activeContext = std::make_unique<ProcessingContextWrapper>();
    // UPDATED: Initialize pending and effective OS configuration
    auto defaultAlgo = apvts.getRawParameterValue("OVERSAMPLING_ALGO")->load();
    auto defaultRate = apvts.getRawParameterValue("OVERSAMPLING_RATE")->load();
    OversamplingAlgorithm initialAlgo = static_cast<OversamplingAlgorithm>(static_cast<int>(defaultAlgo));
    OversamplingRate initialRate = static_cast<OversamplingRate>(static_cast<int>(defaultRate));

    pendingOSAlgo.store(initialAlgo);
    pendingOSRate.store(initialRate);
    // Initialize effective configuration (will be properly set in the first processBlock call)
    effectiveOSAlgo.store(initialAlgo);
    effectiveOSRate.store(initialRate);


    visibleSlotCount.setValue(8);
    fxSlotNodes.resize(maxSlots);
    // ... (Listeners setup remains the same) ...
    for (int i = 0; i < maxSlots; ++i)
        apvts.addParameterListener("SLOT_" + juce::String(i + 1) + "_CHOICE", this);
    // UPDATED: Listen to new parameters
       // apvts.addParameterListener("OVERSAMPLING_CHOICE", this); // REMOVED
    apvts.addParameterListener("OVERSAMPLING_ALGO", this);
    apvts.addParameterListener("OVERSAMPLING_RATE", this);

    apvts.addParameterListener("SAG_ENABLE", this);
    apvts.addParameterListener("INPUT_GAIN", this);
    apvts.addParameterListener("OUTPUT_GAIN", this);
    apvts.addParameterListener("SAG_RESPONSE", this);
}

ModularMultiFxAudioProcessor::~ModularMultiFxAudioProcessor()
{
    for (int i = 0; i < maxSlots; ++i)
        apvts.removeParameterListener("SLOT_" + juce::String(i + 1) + "_CHOICE", this);
    // UPDATED: Remove listeners for new parameters
       // apvts.removeParameterListener("OVERSAMPLING_CHOICE", this); // REMOVED
    apvts.removeParameterListener("OVERSAMPLING_ALGO", this);
    apvts.removeParameterListener("OVERSAMPLING_RATE", this);

    apvts.removeParameterListener("SAG_ENABLE", this);
    // REMOVED: Deprecated SAG listeners

    // NEW: Remove listeners
    apvts.removeParameterListener("INPUT_GAIN", this);
    apvts.removeParameterListener("OUTPUT_GAIN", this);
    apvts.removeParameterListener("SAG_RESPONSE", this);
}

// FIX 2: Updated prepareToPlay to use the new context structure.
void ModularMultiFxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {

    // ✅ FIX G: Store the configuration
    preparedSampleRate = sampleRate;
    preparedMaxBlockSize = samplesPerBlock;

    double safeSampleRate = preparedSampleRate > 0 ? preparedSampleRate : 44100.0;
    int safeSamplesPerBlock = preparedMaxBlockSize > 0 ?
        preparedMaxBlockSize : 512;

    // Determine the required number of channels
    auto numChannels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (numChannels == 0) numChannels = 2; // Safety fallback

    // ✅ FIX D: Dynamic Oversampling Reconfiguration
    // If the channel count changes, we must force a graph update to recreate the OS engines.
    if (numChannels > 0 && currentOSChannels.load() != numChannels)
    {
        // setupOversamplingEngines removed.
        // FIX: Engine creation is dynamic in initiateGraphUpdate.
        currentOSChannels.store(numChannels);
        // Force graph update to pick up new channel count.
        isGraphDirty.store(true);
    }

    // Configure the active graph (initial configuration)
    if (activeContext && activeContext->graph)
        // Note: The actual sample rate and block size for the graph will be set in updateGraph based on OS settings.
        activeContext->graph->setPlayConfigDetails(getTotalNumInputChannels(), getTotalNumOutputChannels(), safeSampleRate, safeSamplesPerBlock);

    // Ensure buffers match the current configuration
    if (numChannels > 0)
    {
        dryBufferForMixing.setSize(numChannels, safeSamplesPerBlock);
        fadeBuffer.setSize(numChannels, safeSamplesPerBlock);
    }

    juce::dsp::ProcessSpec spec{ safeSampleRate, (juce::uint32)safeSamplesPerBlock, (juce::uint32)numChannels };
    // NEW: Prepare Gain Stages
    inputGainStage.prepare(spec);
    outputGainStage.prepare(spec);
    inputGainStage.setRampDurationSeconds(0.01);
    outputGainStage.setRampDurationSeconds(0.01);

    // REMOVED: Initialization of osEngines map.

    smartAutoGain.prepare(spec);
    updateSmartAutoGainParameters();
    updateGainStages();

    // We must ensure the graph is built correctly based on the pending profile.
    isGraphDirty.store(true);
    if (isGraphDirty.load())
    {
        initiateGraphUpdate();
        // In prepareToPlay, we want the configuration applied immediately without crossfade.
        if (fadeState.load() == FadeState::Fading) {
            fadeState.store(FadeState::Idle);
            previousContext.reset();
        }
    }
    reset();
}

// FIX 2: Updated releaseResources and reset
void ModularMultiFxAudioProcessor::releaseResources() {
    if (activeContext && activeContext->graph) activeContext->graph->releaseResources();
    if (previousContext && previousContext->graph) previousContext->graph->releaseResources();
    smartAutoGain.reset();
    inputGainStage.reset();
    outputGainStage.reset();
}

void ModularMultiFxAudioProcessor::reset() {
    if (activeContext && activeContext->graph) activeContext->graph->reset();
    if (previousContext && previousContext->graph) previousContext->graph->reset();
    smartAutoGain.reset();
    inputGainStage.reset();
    outputGainStage.reset();

    // UPDATED: Reset the oversampler owned by the context.
    if (activeContext && activeContext->oversampler) activeContext->oversampler->reset();
    if (previousContext && previousContext->oversampler) previousContext->oversampler->reset();
    // REMOVED: for (auto& pair : osEngines) ...

    fadeState.store(FadeState::Idle);
    fadeSamplesRemaining = 0;
}

// NEW HELPER FUNCTION
void ModularMultiFxAudioProcessor::updateGainStages() {
    // Ensure these parameters exist before accessing them.
 // Using getRawParameterValue for thread safety.
    if (auto* inputParam = apvts.getRawParameterValue("INPUT_GAIN"))
        inputGainStage.setGainDecibels(inputParam->load());
    if (auto* outputParam = apvts.getRawParameterValue("OUTPUT_GAIN"))
        outputGainStage.setGainDecibels(outputParam->load());
}

// UPDATED: New Signal Flow and Fixed Architecture (FIX 2)
void ModularMultiFxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {

    // NEW: Check and update configuration at the start of every block.
 // This handles changes in offline rendering status or module selections efficiently.
    updateOversamplingConfiguration();

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();
    // ... (Buffer clearing, Input Gain, Dry Buffer Copy remains the same as original file) ...
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    juce::dsp::AudioBlock<float> mainBlock(buffer);
    auto mainSubBlock = mainBlock.getSubBlock(0, (size_t)numSamples);
    juce::dsp::ProcessContextReplacing<float> inputContext(mainSubBlock);
    inputGainStage.process(inputContext);
    if (dryBufferForMixing.getNumSamples() < numSamples || dryBufferForMixing.getNumChannels() < buffer.getNumChannels())
    {
        dryBufferForMixing.setSize(buffer.getNumChannels(), numSamples, false, true, true);
    }
    juce::dsp::AudioBlock<float> dryBlock(dryBufferForMixing);
    auto drySubBlock = dryBlock.getSubBlock(0, (size_t)numSamples);
    drySubBlock.copyFrom(mainSubBlock);
    // 3. Process Wet Path (Graph processing)

       // Check if configuration needs update.
    if (isGraphDirty.load())
    {
        initiateGraphUpdate();
    }

    // ✅ FIX 2: Process the context using its associated OS and dedicated buffer.
    auto processContext = [&](ProcessingContextWrapper* context, juce::AudioBuffer<float>& targetBuffer) {
        if (!context || !context->graph) return;
        auto* graph = context->graph.get();
        // UPDATED: Use the unique_ptr from the context.
        auto* oversampler = context->oversampler.get();
        auto& graphWorkBuffer = context->oversampledGraphBuffer;
        // Use the dedicated buffer

        if (oversampler != nullptr)
        {
            // 1. Upsample
            juce::dsp::AudioBlock<float> mainBlock(targetBuffer);
            auto upsampledBlock = oversampler->processSamplesUp(mainBlock);

            int requiredSamples = (int)upsampledBlock.getNumSamples();
            int numChannels = (int)upsampledBlock.getNumChannels();
            // 2. Safety check for buffer size (using the dedicated buffer)
            if (graphWorkBuffer.getNumSamples() < requiredSamples || graphWorkBuffer.getNumChannels() < numChannels)
            {
                // Fallback resize if updateGraph didn't size it correctly (should not happen).
                jassertfalse;
                graphWorkBuffer.setSize(numChannels, requiredSamples, false, true, true);
            }

            // 3. Create wrapper for the dedicated buffer.
            juce::AudioBuffer<float> graphBuffer(graphWorkBuffer.getArrayOfWritePointers(), numChannels, 0, requiredSamples);

            // 4. Copy input to dedicated buffer.
            juce::dsp::AudioBlock<float>(graphBuffer).copyFrom(upsampledBlock);
            // 5. Process the graph.
            graph->processBlock(graphBuffer, midiMessages);
            // 6. Copy output from dedicated buffer.
            upsampledBlock.copyFrom(juce::dsp::AudioBlock<float>(graphBuffer));

            // 7. Downsample.
            oversampler->processSamplesDown(mainBlock);
        }
        else
        {
            graph->processBlock(targetBuffer, midiMessages);
        }
        };

    if (fadeState.load() == FadeState::Fading && previousContext)
    {
        // (Copy input to fadeBuffer)
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            if (ch < fadeBuffer.getNumChannels())
                fadeBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }

        // Process using the respective contexts
        processContext(previousContext.get(), fadeBuffer);
        processContext(activeContext.get(), buffer);

        int samplesToFade = std::min(numSamples, fadeSamplesRemaining);
        // ... (Crossfade mixing logic remains the same as original file) ...
        for (int i = 0; i < samplesToFade; ++i)
        {
            float fade = (float)(totalFadeSamples - (fadeSamplesRemaining - i)) / (float)totalFadeSamples;
            float gainIn = fade;
            float gainOut = 1.0f - fade;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                if (ch < fadeBuffer.getNumChannels()) {
                    float oldSample = fadeBuffer.getSample(ch, i) * gainOut;
                    float newSample = buffer.getSample(ch, i) * gainIn;
                    buffer.setSample(ch, i, oldSample + newSample);
                }
            }
        }

        fadeSamplesRemaining -= samplesToFade;
        if (fadeSamplesRemaining <= 0)
        {
            fadeState.store(FadeState::Idle);
            previousContext.reset(); // Release the old context and its buffer
        }
    }
    else
    {
        processContext(activeContext.get(), buffer);
    }

    // 4. SmartAutoGain processing ... (Rest of processBlock remains the same as original file) ...
    smartAutoGain.process(drySubBlock, mainSubBlock);
    juce::dsp::ProcessContextReplacing<float> outputContext(mainSubBlock);
    outputGainStage.process(outputContext);
    auto mixValue = apvts.getRawParameterValue("MASTER_MIX")->load();
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        if (ch < dryBufferForMixing.getNumChannels())
        {
            buffer.applyGain(ch, 0, numSamples, mixValue);
            buffer.addFrom(ch, 0, dryBufferForMixing, ch, 0, numSamples, 1.0f - mixValue);
        }
    }
}

// FIX 2: Updated getTailLengthSeconds
double ModularMultiFxAudioProcessor::getTailLengthSeconds() const {
    if (!activeContext || !activeContext->graph) return 0.0;
    double tail = activeContext->graph->getTailLengthSeconds();
    // UPDATED: Use the unique_ptr.
    auto* oversampler = activeContext->oversampler.get();
    if (oversampler != nullptr && oversampler->getOversamplingFactor() > 1.0)
        tail /= oversampler->getOversamplingFactor();
    return tail;
}

juce::AudioProcessorValueTreeState::ParameterLayout ModularMultiFxAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // UPDATED: Added "Spectral Animator". Indices updated.
 // New indices: 0:Empty..9:Physical Resonator, 10:Spectral Animator
    auto fxChoices = juce::StringArray{ "Empty", "Distortion", "Filter", "Modulation", "Delay", "Reverb", "Compressor",
                                        "ChromaTape", "MorphoComp", "Physical Resonator", "Spectral Animator" };
    for (int i = 0; i < maxSlots; ++i)
    {
        auto slotId = "SLOT_" + juce::String(i + 1);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotId + "_CHOICE", slotId + " FX", fxChoices, 0));

        auto slotPrefix = "SLOT_" + juce::String(i + 1) + "_";
        // ... (Existing module parameters remain the same - omitted for brevity) ...
        auto distortionTypes = juce::StringArray{ "Vintage Tube", "Op-Amp", "Germanium Fuzz" };
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_DRIVE", "Drive", 0.0f, 24.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_LEVEL", "Level", -24.0f, 24.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "DISTORTION_TYPE", "Type", distortionTypes, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_BIAS", "Bias", -1.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_CHARACTER", "Character", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "FILTER_PROFILE", "Profile", juce::StringArray{ "SVF", "Transistor Ladder", "Diode Ladder", "OTA" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "FILTER_CUTOFF", "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 1000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "FILTER_RESONANCE", "Resonance", 0.1f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "FILTER_DRIVE", "Drive", 1.0f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "FILTER_TYPE", "SVF Type", juce::StringArray{ "Low-Pass", "Band-Pass", "High-Pass" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "MODULATION_MODE", "Mode", juce::StringArray{ "Chorus", "Flanger", "Vibrato", "Phaser" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_RATE", "Rate", 0.01f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_DEPTH", "Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_FEEDBACK", "Feedback", -0.95f, 0.95f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_MIX", "Mix", 0.0f, 1.0f, 0.5f));

        auto advDelayPrefix = slotPrefix + "ADVDELAY_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(advDelayPrefix + "MODE", "Mode", juce::StringArray{ "Tape", "BBD", "Digital" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "TIME", "Time (ms)", juce::NormalisableRange<float>(1.0f, 2000.0f, 0.1f, 0.5f), 500.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "FEEDBACK", "Feedback", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "MIX", "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "COLOR", "Color", juce::NormalisableRange<float>(200.0f, 15000.0f, 0.0f, 0.3f), 5000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "WOW", "Wow", 0.0f, 1.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "FLUTTER", "Flutter", 0.0f, 1.0f, 0.1f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "AGE", "Age", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_ROOM_SIZE", "Room Size", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_DAMPING", "Damping", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_MIX", "Mix", 0.0f, 1.0f, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_WIDTH", "Width", 0.0f, 1.0f, 1.0f));
        auto advCompPrefix = slotPrefix + "ADVCOMP_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(advCompPrefix + "TOPOLOGY", "Topology", juce::StringArray{ "VCA Clean", "FET Aggressive", "Opto Smooth" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(advCompPrefix + "DETECTOR", "Detector", juce::StringArray{ "Peak", "RMS" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "THRESHOLD", "Threshold", -60.0f, 0.0f, -12.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "RATIO", "Ratio", 1.0f, 20.0f, 4.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "ATTACK", "Attack (ms)", juce::NormalisableRange<float>(0.1f, 500.0f, 0.0f, 0.3f), 20.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "RELEASE", "Release (ms)", juce::NormalisableRange<float>(10.0f, 2000.0f, 0.0f, 0.3f), 200.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "MAKEUP", "Makeup Gain", 0.0f, 24.0f, 0.0f));
        auto ctPrefix = slotPrefix + "CT_";
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "LOWMID_CROSS", "Low/Mid X-Over", juce::NormalisableRange<float>(50.0f, 1000.0f, 1.0f, 0.3f), 250.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "MIDHIGH_CROSS", "Mid/High X-Over", juce::NormalisableRange<float>(1000.0f, 10000.0f, 1.0f, 0.3f), 3000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "SCRAPE_FLUTTER", "Scrape Flutter", 0.0f, 1.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "CHAOS_AMOUNT", "Chaos Amount", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HISS_LEVEL", "Hiss Level (dB)", juce::NormalisableRange<float>(-120.0f, -40.0f, 0.1f), -120.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HUM_LEVEL", "Hum Level (dB)", juce::NormalisableRange<float>(-120.0f, -50.0f, 0.1f), -120.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HEADBUMP_FREQ", "Head Bump Freq", juce::NormalisableRange<float>(40.0f, 140.0f, 1.0f, 0.5f), 80.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HEADBUMP_GAIN", "Head Bump Gain (dB)", 0.0f, 6.0f, 3.0f));
        juce::StringArray bandNames = { "LOW", "MID", "HIGH" };
        for (const auto& band : bandNames)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + band + "_SATURATION", band + " Saturation", 0.0f, 12.0f, 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + band + "_WOW", band + " Wow", 0.0f, 1.0f, 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + band + "_FLUTTER", band + " Flutter", 0.0f, 1.0f, 0.0f));
        }

        // MorphoComp Parameters (remain)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_AMOUNT", "Amount", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_RESPONSE", "Response", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "MORPHO_MODE", "Mode", juce::StringArray{ "Auto", "Manual" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_X", "Morph X", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_Y", "Morph Y", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_MIX", "Mix", 0.0f, 1.0f, 1.0f));

        // Physical Resonator Parameters (UPDATED)
        auto physResPrefix = slotPrefix + "PHYSRES_";
        // UPDATED: Model choices changed to match the request: "Modal", "Sympathetic", "String"
        params.push_back(std::make_unique<juce::AudioParameterChoice>(physResPrefix + "MODEL", "Model", juce::StringArray{ "Modal", "Sympathetic", "String" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "TUNE", "Tune", juce::NormalisableRange<float>(20.0f, 5000.0f, 0.0f, 0.25f), 220.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "STRUCTURE", "Structure", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "BRIGHTNESS", "Brightness", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "DAMPING", "Damping", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "POSITION", "Position", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "EXCITE_TYPE", "Excite Type", 0.0f, 1.0f, 0.8f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "SENSITIVITY", "Sensitivity", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "MIX", "Mix", 0.0f, 1.0f, 1.0f));
        // NEW: Excitation Engine Advanced Parameters (ADSR and Noise Type)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(physResPrefix + "NOISE_TYPE", "Noise Type", juce::StringArray{ "White", "Pink" }, 0));
        // ADSR times in seconds. Using logarithmic scale (0.3f skew factor) for time parameters.
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "ATTACK", "Attack", juce::NormalisableRange<float>(0.001f, 1.0f, 0.0f, 0.3f), 0.001f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "DECAY", "Decay", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.3f), 0.05f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "SUSTAIN", "Sustain", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "RELEASE", "Release", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.3f), 0.01f));
        // NEW: Spectral Animator Parameters (Using prefix SPECANIM_)
        auto specAnimPrefix = slotPrefix + "SPECANIM_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(specAnimPrefix + "MODE", "Mode", juce::StringArray{ "Pitch", "Formant" }, 0));
        // Pitch (Hz): 50Hz to 2000Hz (Logarithmic scale)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "PITCH", "Pitch (Hz)", juce::NormalisableRange<float>(50.0f, 2000.0f, 0.1f, 0.3f), 440.0f));
        // Formant X/Y (0-1)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "FORMANT_X", "Formant X (Back/Front)", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "FORMANT_Y", "Formant Y (Close/Open)", 0.0f, 1.0f, 0.5f));
        // Morph (0-1)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "MORPH", "Morph", 0.0f, 1.0f, 1.0f));
        // Transient Preservation (0-1)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "TRANSIENT_PRESERVE", "Transients", 0.0f, 1.0f, 0.8f));
    }

    // Global Parameters (remain)
    params.push_back(std::make_unique<juce::AudioParameterChoice>("OVERSAMPLING_ALGO", "OS Algorithm", juce::StringArray{ "Live (IIR)", "HQ (FIR)", "Deluxe (FIR)" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("OVERSAMPLING_RATE", "OS Rate", juce::StringArray{ "1x (Off)", "2x", "4x", "8x", "16x" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MASTER_MIX", "Master Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("INPUT_GAIN", "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("OUTPUT_GAIN", "Output Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("SAG_ENABLE", "Auto-Gain", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SAG_RESPONSE", "Response (ms)",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.5f), 50.0f));
    return { params.begin(), params.end() };
}
// ... (getStateInformation, setStateInformation, checkForChromaTapeUsage, updateOversamplingConfiguration, initiateGraphUpdate, updateGraph remain the same)
void ModularMultiFxAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty("visibleSlotCount", visibleSlotCount.getValue(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ModularMultiFxAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            visibleSlotCount.setValue(apvts.state.getProperty("visibleSlotCount", 8));
        }
}

// NEW HELPER FUNCTION: Check if ChromaTape (Choice 7) is active in any visible slot.
bool ModularMultiFxAudioProcessor::checkForChromaTapeUsage() const
{
    int numVisible = visibleSlotCount.getValue();
    for (int i = 0; i < numVisible; ++i)
    {
        auto choiceParamId = "SLOT_" + juce::String(i + 1) + "_CHOICE";
        auto* choiceParam = apvts.getRawParameterValue(choiceParamId);
        if (choiceParam != nullptr && static_cast<int>(choiceParam->load()) == 7)
        {
            return true;
        }
    }
    return false;
}

// NEW HELPER FUNCTION: Centralized logic for determining OS settings.
void ModularMultiFxAudioProcessor::updateOversamplingConfiguration()
{
    OversamplingAlgorithm newAlgo = pendingOSAlgo.load();
    OversamplingRate newRate = pendingOSRate.load();
    bool shouldLock = false;
    if (isNonRealtime())
    {
        newAlgo = OversamplingAlgorithm::Deluxe;
        newRate = OversamplingRate::x8;
    }
    else if (checkForChromaTapeUsage())
    {
        shouldLock = true;
        if (newRate > OversamplingRate::x2)
        {
            newRate = OversamplingRate::x2;
        }
    }

    bool configChanged = (effectiveOSAlgo.load() != newAlgo || effectiveOSRate.load() != newRate);
    bool lockStatusChanged = (oversamplingLockActive.load() != shouldLock);

    if (configChanged)
    {
        effectiveOSAlgo.store(newAlgo);
        effectiveOSRate.store(newRate);
        isGraphDirty.store(true);
    }

    if (lockStatusChanged)
    {
        oversamplingLockActive.store(shouldLock);
        juce::MessageManager::callAsync([this] {
            if (this != nullptr) {
                osLockChangeBroadcaster.sendChangeMessage();
            }
            });
    }
}

// FIX 2: Updated initiateGraphUpdate
void ModularMultiFxAudioProcessor::initiateGraphUpdate() {
    if (previousContext)
    {
        if (previousContext->graph) previousContext->graph->releaseResources();
        previousContext.reset();
    }

    previousContext = std::move(activeContext);
    activeContext = std::make_unique<ProcessingContextWrapper>();

    OversamplingAlgorithm newAlgo = effectiveOSAlgo.load();
    OversamplingRate newRate = effectiveOSRate.load();
    int numChannels = currentOSChannels.load();
    if (numChannels == 0) {
        numChannels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
        if (numChannels == 0) numChannels = 2;
    }

    if (numChannels > 0)
    {
        activeContext->oversampler = createOversamplingEngine(newRate, newAlgo, numChannels);
    }

    if (activeContext->oversampler)
    {
        if (preparedMaxBlockSize > 0)
        {
            activeContext->oversampler->initProcessing(preparedMaxBlockSize);
        }
        activeContext->oversampler->reset();
    }

    if (updateGraph())
    {
        if (getSampleRate() > 0 && previousContext)
        {
            int fadeSamples = (int)(getSampleRate() * crossfadeDurationMs / 1000.0);
            totalFadeSamples = std::max(1, fadeSamples);
            fadeSamplesRemaining = totalFadeSamples;
            fadeState.store(FadeState::Fading);
        }
        isGraphDirty.store(false);
        int totalLatency = 0;
        if (activeContext->oversampler)
            totalLatency = (int)activeContext->oversampler->getLatencyInSamples();
        setLatencySamples(totalLatency);
    }
}

// FIX 1 & 2: Updated updateGraph
bool ModularMultiFxAudioProcessor::updateGraph() {
    if (preparedSampleRate <= 0 || preparedMaxBlockSize <= 0) {
        return false;
    }

    if (!activeContext || !activeContext->graph) return false;
    int numChannels = currentOSChannels.load();
    if (numChannels == 0) {
        numChannels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
        if (numChannels == 0) numChannels = 2;
    }

    activeContext->graph->clear();
    double graphSampleRate = preparedSampleRate;
    int graphBlockSize = preparedMaxBlockSize;
    if (activeContext->oversampler)
    {
        graphSampleRate *= activeContext->oversampler->getOversamplingFactor();
        graphBlockSize = (int)((double)preparedMaxBlockSize * activeContext->oversampler->getOversamplingFactor());
    }

    if (numChannels > 0 && activeContext->oversampler)
    {
        activeContext->oversampledGraphBuffer.setSize(numChannels, graphBlockSize);
    }
    else
    {
        activeContext->oversampledGraphBuffer.setSize(0, 0);
    }

    activeContext->graph->setPlayConfigDetails(numChannels, numChannels, graphSampleRate, graphBlockSize);
    inputNode = activeContext->graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    outputNode = activeContext->graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    auto connectNodes = [&](juce::AudioProcessorGraph::Node* sourceNode, juce::AudioProcessorGraph::Node* destNode) {
        if (!sourceNode || !destNode) return;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (activeContext->graph->canConnect({ { sourceNode->nodeID, ch }, { destNode->nodeID, ch } }))
            {
                activeContext->graph->addConnection({ { sourceNode->nodeID, ch }, { destNode->nodeID, ch } });
            }
        }
        };
    juce::AudioProcessorGraph::Node* lastNode = inputNode.get();
    bool moduleAdded = false;

    int numVisible = visibleSlotCount.getValue();
    const int numCols = 4;
    int slotsConsumed = 0;
    for (int i = 0; i < numVisible; i += slotsConsumed)
    {
        slotsConsumed = 1;
        auto choiceParamId = "SLOT_" + juce::String(i + 1) + "_CHOICE";
        auto* choiceParam = apvts.getRawParameterValue(choiceParamId);
        int choice = (choiceParam != nullptr) ? static_cast<int>(choiceParam->load()) : 0;

        if (choice == 7) {
            slotsConsumed = 3;
        }

        int currentCol = i % numCols;
        if (currentCol + slotsConsumed > numCols)
        {
            slotsConsumed = std::max(1, numCols - currentCol);
        }

        if (i + slotsConsumed > numVisible)
        {
            slotsConsumed = numVisible - i;
        }

        if (slotsConsumed <= 0) break;
        if (choice > 0)
        {
            fxSlotNodes[i] = activeContext->graph->addNode(createProcessorForChoice(choice, i));
            if (fxSlotNodes[i])
            {
                connectNodes(lastNode, fxSlotNodes[i].get());
                lastNode = fxSlotNodes[i].get();
                moduleAdded = true;
            }

            for (int j = 1; j < slotsConsumed; ++j)
            {
                if (i + j < maxSlots) {
                    fxSlotNodes[i + j] = nullptr;
                }
            }
        }
        else
        {
            for (int j = 0; j < slotsConsumed; ++j)
            {
                if (i + j < maxSlots) {
                    fxSlotNodes[i + j] = nullptr;
                }
            }
        }
    }

    if (!moduleAdded)
    {
        auto wireNode = activeContext->graph->addNode(std::make_unique<PassThroughProcessor>());
        if (wireNode)
        {
            connectNodes(lastNode, wireNode.get());
            lastNode = wireNode.get();
        }
    }

    connectNodes(lastNode, outputNode.get());
    activeContext->graph->prepareToPlay(graphSampleRate, graphBlockSize);
    for (auto node : activeContext->graph->getNodes())
    {
        if (node != nullptr && node->getProcessor() != nullptr)
            node->getProcessor()->enableAllBuses();
    }

    return true;
}

std::unique_ptr<juce::AudioProcessor> ModularMultiFxAudioProcessor::createProcessorForChoice(int choice, int slotIndex) {
    switch (choice)
    {
    case 1: return std::make_unique<DistortionProcessor>(apvts, slotIndex);
    case 2: return std::make_unique<FilterProcessor>(apvts, slotIndex);
    case 3: return std::make_unique<ModulationProcessor>(apvts, slotIndex);
    case 4: return std::make_unique<AdvancedDelayProcessor>(apvts, slotIndex);
    case 5: return std::make_unique<ReverbProcessor>(apvts, slotIndex);
    case 6: return std::make_unique<AdvancedCompressorProcessor>(apvts, slotIndex);
    case 7: return std::make_unique<ChromaTapeProcessor>(apvts, slotIndex);
        // REMOVED: Case 8 (BBDCloud)
               // REMOVED: Case 9 (FractureTube)
    case 8: return std::make_unique<MorphoCompProcessor>(apvts, slotIndex);
        // Renumbered from 10
    case 9: return std::make_unique<PhysicalResonatorProcessor>(apvts, slotIndex);
        // Renumbered from 11
               // NEW CASE
    case 10: return std::make_unique<SpectralAnimatorProcessor>(apvts, slotIndex);
    default: return nullptr;
    }
}

void ModularMultiFxAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID.endsWith("_CHOICE") && parameterID.startsWith("SLOT_"))
    {
        isGraphDirty.store(true);
        editorResizeBroadcaster.sendChangeMessage();
    }

    if (parameterID == "OVERSAMPLING_ALGO")
    {
        OversamplingAlgorithm newAlgo = static_cast<OversamplingAlgorithm>(static_cast<int>(newValue));
        pendingOSAlgo.store(newAlgo);
    }
    else if (parameterID == "OVERSAMPLING_RATE")
    {
        OversamplingRate newRate = static_cast<OversamplingRate>(static_cast<int>(newValue));
        pendingOSRate.store(newRate);
    }

    if (parameterID.startsWith("SAG_"))
        updateSmartAutoGainParameters();
    if (parameterID == "INPUT_GAIN" || parameterID == "OUTPUT_GAIN")
        updateGainStages();
}

void ModularMultiFxAudioProcessor::updateSmartAutoGainParameters() {
    if (auto* pEnable = apvts.getRawParameterValue("SAG_ENABLE"))
        smartAutoGain.setEnabled(pEnable->load() > 0.5f);
    if (auto* pResponse = apvts.getRawParameterValue("SAG_RESPONSE"))
    {
        smartAutoGain.setResponseTime(pResponse->load());
    }
}

std::unique_ptr<juce::dsp::Oversampling<float>> ModularMultiFxAudioProcessor::createOversamplingEngine(OversamplingRate rate, OversamplingAlgorithm algo, int numChannels)
{
    if (numChannels <= 0 || rate == OversamplingRate::x1)
        return nullptr;
    int stages = 0;
    switch (rate)
    {
    case OversamplingRate::x2: stages = 1; break;
    case OversamplingRate::x4: stages = 2; break;
    case OversamplingRate::x8: stages = 3; break;
    case OversamplingRate::x16: stages = 4; break;
    default: return nullptr;
    }

    juce::dsp::Oversampling<float>::FilterType filterType;
    bool useLinearPhase = true;
    switch (algo)
    {
    case OversamplingAlgorithm::Live:
        filterType = juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR;
        useLinearPhase = false;
        break;
    case OversamplingAlgorithm::HQ:
    case OversamplingAlgorithm::Deluxe:
        filterType = juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;
        useLinearPhase = true;
        break;
    }

    return std::make_unique<juce::dsp::Oversampling<float>>(
        (size_t)numChannels,
        stages,
        filterType,
        useLinearPhase
    );
}

juce::AudioProcessorEditor* ModularMultiFxAudioProcessor::createEditor() {
    return new ModularMultiFxAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ModularMultiFxAudioProcessor();
}