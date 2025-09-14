//================================================================================
// File: PluginProcessor.cpp
//================================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

// Include all FX module headers
#include "FX_Modules/DistortionProcessor.h"
#include "FX_Modules/FilterProcessor.h"
#include "FX_Modules/ModulationProcessor.h"
#include "FX_Modules/AdvancedDelayProcessor.h"
#include "FX_Modules/ReverbProcessor.h"
#include "FX_Modules/AdvancedCompressorProcessor.h"
#include "FX_Modules/ChromaTapeProcessor.h"
#include "FX_Modules/MorphoCompProcessor.h"
#include "FX_Modules/PhysicalResonatorProcessor.h"
#include "FX_Modules/SpectralAnimatorProcessor.h"
#include "FX_Modules/HelicalDelayProcessor.h"
#include "FX_Modules/ChronoVerbProcessor.h"
#include "FX_Modules/TectonicDelayProcessor.h"

// A simple processor to pass audio through when no other module is loaded.
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
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ignoreUnused(midi);
        juce::ScopedNoDenormals noDenormals;
        for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear(ch, 0, buffer.getNumSamples());
    }
    const juce::String getName() const override { return "PassThrough"; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PassThroughProcessor)
};


ModularMultiFxAudioProcessor::ModularMultiFxAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    presetManager = std::make_unique<PresetManager>(apvts, *this, "Tessera");
    activeContext = std::make_unique<ProcessingContextWrapper>();

    auto defaultAlgo = apvts.getRawParameterValue("OVERSAMPLING_ALGO")->load();
    auto defaultRate = apvts.getRawParameterValue("OVERSAMPLING_RATE")->load();
    auto initialAlgo = static_cast<OversamplingAlgorithm>((int)defaultAlgo);
    auto initialRate = static_cast<OversamplingRate>((int)defaultRate);
    pendingOSAlgo.store(initialAlgo);
    pendingOSRate.store(initialRate);
    effectiveOSAlgo.store(initialAlgo);
    effectiveOSRate.store(initialRate);

    fxSlotNodes.resize(maxSlots);
    for (int i = 0; i < maxSlots; ++i)
        apvts.addParameterListener("SLOT_" + juce::String(i + 1) + "_CHOICE", this);

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

    apvts.removeParameterListener("OVERSAMPLING_ALGO", this);
    apvts.removeParameterListener("OVERSAMPLING_RATE", this);
    apvts.removeParameterListener("SAG_ENABLE", this);
    apvts.removeParameterListener("INPUT_GAIN", this);
    apvts.removeParameterListener("OUTPUT_GAIN", this);
    apvts.removeParameterListener("SAG_RESPONSE", this);
}

void ModularMultiFxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = sampleRate;
    preparedMaxBlockSize = samplesPerBlock;

    double safeSR = preparedSampleRate > 0 ? preparedSampleRate : 44100.0;
    int safeBS = preparedMaxBlockSize > 0 ? preparedMaxBlockSize : 512;
    int numChannels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (numChannels == 0) numChannels = 2;

    if (numChannels > 0 && currentOSChannels.load() != numChannels)
    {
        currentOSChannels.store(numChannels);
        isGraphDirty.store(true);
    }

    if (activeContext && activeContext->graph)
        activeContext->graph->setPlayConfigDetails(getTotalNumInputChannels(), getTotalNumOutputChannels(), safeSR, safeBS);

    if (numChannels > 0)
    {
        dryBufferForMixing.setSize(numChannels, safeBS);
        fadeBuffer.setSize(numChannels, safeBS);
    }

    juce::dsp::ProcessSpec spec{ safeSR, (juce::uint32)safeBS, (juce::uint32)numChannels };
    inputGainStage.prepare(spec);
    outputGainStage.prepare(spec);
    inputGainStage.setRampDurationSeconds(0.01);
    outputGainStage.setRampDurationSeconds(0.01);
    smartAutoGain.prepare(spec);
    updateSmartAutoGainParameters();
    updateGainStages();

    isGraphDirty.store(true);
    if (isGraphDirty.load())
    {
        initiateGraphUpdate();
        if (fadeState.load() == FadeState::Fading)
        {
            fadeState.store(FadeState::Idle);
            previousContext.reset();
        }
    }

    reset();
}

void ModularMultiFxAudioProcessor::releaseResources()
{
    if (activeContext && activeContext->graph) activeContext->graph->releaseResources();
    if (previousContext && previousContext->graph) previousContext->graph->releaseResources();
    smartAutoGain.reset();
    inputGainStage.reset();
    outputGainStage.reset();
}

void ModularMultiFxAudioProcessor::reset()
{
    if (activeContext && activeContext->graph) activeContext->graph->reset();
    if (previousContext && previousContext->graph) previousContext->graph->reset();
    smartAutoGain.reset();
    inputGainStage.reset();
    outputGainStage.reset();
    if (activeContext && activeContext->oversampler) activeContext->oversampler->reset();
    if (previousContext && previousContext->oversampler) previousContext->oversampler->reset();
    fadeState.store(FadeState::Idle);
    fadeSamplesRemaining = 0;
}

void ModularMultiFxAudioProcessor::updateGainStages()
{
    if (auto* pIn = apvts.getRawParameterValue("INPUT_GAIN"))
        inputGainStage.setGainDecibels(pIn->load());
    if (auto* pOut = apvts.getRawParameterValue("OUTPUT_GAIN"))
        outputGainStage.setGainDecibels(pOut->load());
}

void ModularMultiFxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    updateOversamplingConfiguration();
    juce::ScopedNoDenormals noDenormals;

    auto totalIn = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    for (auto ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    juce::dsp::AudioBlock<float> block(buffer);
    auto subBlock = block.getSubBlock(0, (size_t)numSamples);
    juce::dsp::ProcessContextReplacing<float> inCtx(subBlock);
    inputGainStage.process(inCtx);

    if (dryBufferForMixing.getNumSamples() < numSamples || dryBufferForMixing.getNumChannels() < buffer.getNumChannels())
        dryBufferForMixing.setSize(buffer.getNumChannels(), numSamples, false, true, true);

    juce::dsp::AudioBlock<float> dryBlock(dryBufferForMixing);
    dryBlock.getSubBlock(0, (size_t)numSamples).copyFrom(subBlock);

    if (isGraphDirty.load())
        initiateGraphUpdate();

    auto processCtx = [&](ProcessingContextWrapper* ctx, juce::AudioBuffer<float>& tgt)
        {
            if (!ctx || !ctx->graph) return;
            auto* graph = ctx->graph.get();
            auto* oversampler = ctx->oversampler.get();
            auto& osBuffer = ctx->oversampledGraphBuffer;

            if (oversampler != nullptr)
            {
                juce::dsp::AudioBlock<float> mainBlock(tgt);
                auto upBlock = oversampler->processSamplesUp(mainBlock);
                int required = (int)upBlock.getNumSamples();
                int chans = (int)upBlock.getNumChannels();
                if (osBuffer.getNumSamples() < required || osBuffer.getNumChannels() < chans)
                    osBuffer.setSize(chans, required, false, true, true);
                juce::AudioBuffer<float> graphBuf(osBuffer.getArrayOfWritePointers(), chans, 0, required);
                juce::dsp::AudioBlock<float>(graphBuf).copyFrom(upBlock);
                graph->processBlock(graphBuf, midi);
                upBlock.copyFrom(juce::dsp::AudioBlock<float>(graphBuf));
                oversampler->processSamplesDown(mainBlock);
            }
            else
            {
                graph->processBlock(tgt, midi);
            }
        };

    if (fadeState.load() == FadeState::Fading && previousContext)
    {
        for (int ch = 0; ch < totalIn; ++ch)
            if (ch < fadeBuffer.getNumChannels())
                fadeBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        processCtx(previousContext.get(), fadeBuffer);
        processCtx(activeContext.get(), buffer);

        int samplesToFade = std::min(numSamples, fadeSamplesRemaining);
        for (int i = 0; i < samplesToFade; ++i)
        {
            float fade = (float)(totalFadeSamples - (fadeSamplesRemaining - i)) / (float)totalFadeSamples;
            float gIn = fade;
            float gOut = 1.0f - fade;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                if (ch < fadeBuffer.getNumChannels())
                {
                    float oldS = fadeBuffer.getSample(ch, i) * gOut;
                    float newS = buffer.getSample(ch, i) * gIn;
                    buffer.setSample(ch, i, oldS + newS);
                }
            }
        }
        fadeSamplesRemaining -= samplesToFade;
        if (fadeSamplesRemaining <= 0)
        {
            fadeState.store(FadeState::Idle);
            previousContext.reset();
        }
    }
    else
    {
        processCtx(activeContext.get(), buffer);
    }

    smartAutoGain.process(dryBlock.getSubBlock(0, (size_t)numSamples), subBlock);
    juce::dsp::ProcessContextReplacing<float> outCtx(subBlock);
    outputGainStage.process(outCtx);

    float masterMix = apvts.getRawParameterValue("MASTER_MIX")->load();
    for (int ch = 0; ch < totalOut; ++ch)
    {
        if (ch < dryBufferForMixing.getNumChannels())
        {
            buffer.applyGain(ch, 0, numSamples, masterMix);
            buffer.addFrom(ch, 0, dryBufferForMixing, ch, 0, numSamples, 1.0f - masterMix);
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ModularMultiFxAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto fxChoices = juce::StringArray{ "Empty", "Distortion", "Filter", "Modulation", "Delay", "Reverb", "Compressor", "ChromaTape", "MorphoComp", "Physical Resonator", "Spectral Animator", "Helical Delay", "Chrono-Verb", "Tectonic Delay" };

    for (int i = 0; i < maxSlots; ++i)
    {
        auto slotId = "SLOT_" + juce::String(i + 1);
        auto slotPrefix = slotId + "_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotId + "_CHOICE", slotId + " FX", fxChoices, 0));

        // Distortion
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_DRIVE", "Drive", 0.0f, 24.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_LEVEL", "Level", -24.0f, 24.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "DISTORTION_TYPE", "Type", juce::StringArray{ "Vintage Tube", "Op-Amp", "Germanium Fuzz" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_BIAS", "Bias", -1.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "DISTORTION_CHARACTER", "Character", 0.0f, 1.0f, 0.5f));

        // Filter
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "FILTER_PROFILE", "Profile", juce::StringArray{ "SVF", "Transistor Ladder", "Diode Ladder", "OTA" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "FILTER_CUTOFF", "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 1000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "FILTER_RESONANCE", "Resonance", 0.1f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "FILTER_DRIVE", "Drive", 1.0f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "FILTER_TYPE", "SVF Type", juce::StringArray{ "Low-Pass", "Band-Pass", "High-Pass" }, 0));

        // Modulation
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "MODULATION_MODE", "Mode", juce::StringArray{ "Chorus", "Flanger", "Vibrato", "Phaser" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_RATE", "Rate", 0.01f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_DEPTH", "Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_FEEDBACK", "Feedback", -0.95f, 0.95f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MODULATION_MIX", "Mix", 0.0f, 1.0f, 0.5f));

        // Advanced Delay
        auto advDelayPrefix = slotPrefix + "ADVDELAY_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(advDelayPrefix + "MODE", "Mode", juce::StringArray{ "Tape", "BBD", "Digital" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "TIME", "Time (ms)", juce::NormalisableRange<float>(1.0f, 2000.0f, 0.1f, 0.5f), 500.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "FEEDBACK", "Feedback", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "MIX", "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "COLOR", "Color", juce::NormalisableRange<float>(200.0f, 15000.0f, 0.0f, 0.3f), 5000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "WOW", "Wow", 0.0f, 1.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "FLUTTER", "Flutter", 0.0f, 1.0f, 0.1f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advDelayPrefix + "AGE", "Age", 0.0f, 1.0f, 0.5f));

        // Reverb
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_ROOM_SIZE", "Room Size", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_DAMPING", "Damping", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_MIX", "Mix", 0.0f, 1.0f, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "REVERB_WIDTH", "Width", 0.0f, 1.0f, 1.0f));

        // Advanced Compressor
        auto advCompPrefix = slotPrefix + "ADVCOMP_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(advCompPrefix + "TOPOLOGY", "Topology", juce::StringArray{ "VCA Clean", "FET Aggressive", "Opto Smooth" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(advCompPrefix + "DETECTOR", "Detector", juce::StringArray{ "Peak", "RMS" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "THRESHOLD", "Threshold", -60.0f, 0.0f, -12.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "RATIO", "Ratio", 1.0f, 20.0f, 4.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "ATTACK", "Attack (ms)", juce::NormalisableRange<float>(0.1f, 500.0f, 0.0f, 0.3f), 20.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "RELEASE", "Release (ms)", juce::NormalisableRange<float>(10.0f, 2000.0f, 0.0f, 0.3f), 200.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(advCompPrefix + "MAKEUP", "Makeup Gain", 0.0f, 24.0f, 0.0f));

        // ChromaTape
        auto ctPrefix = slotPrefix + "CT_";
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "LOWMID_CROSS", "Low/Mid X-Over", juce::NormalisableRange<float>(50.0f, 1000.0f, 1.0f, 0.3f), 250.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "MIDHIGH_CROSS", "Mid/High X-Over", juce::NormalisableRange<float>(1000.0f, 10000.0f, 1.0f, 0.3f), 3000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "SCRAPE_FLUTTER", "Scrape Flutter", 0.0f, 1.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "CHAOS_AMOUNT", "Chaos Amount", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HISS_LEVEL", "Hiss Level (dB)", juce::NormalisableRange<float>(-120.0f, -40.0f, 0.1f), -120.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HUM_LEVEL", "Hum Level (dB)", juce::NormalisableRange<float>(-120.0f, -50.0f, 0.1f), -120.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HEADBUMP_FREQ", "Head Bump Freq", juce::NormalisableRange<float>(40.0f, 140.0f, 1.0f, 0.5f), 80.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + "HEADBUMP_GAIN", "Head Bump Gain (dB)", 0.0f, 6.0f, 3.0f));
        for (auto band : { "LOW", "MID", "HIGH" })
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + band + juce::String("_SATURATION"), juce::String(band) + " Saturation", 0.0f, 12.0f, 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + band + juce::String("_WOW"), juce::String(band) + " Wow", 0.0f, 1.0f, 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(ctPrefix + band + juce::String("_FLUTTER"), juce::String(band) + " Flutter", 0.0f, 1.0f, 0.0f));
        }

        // MorphoComp
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_AMOUNT", "Amount", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_RESPONSE", "Response", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(slotPrefix + "MORPHO_MODE", "Mode", juce::StringArray{ "Auto", "Manual" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_X", "Morph X", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_Y", "Morph Y", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(slotPrefix + "MORPHO_MIX", "Mix", 0.0f, 1.0f, 1.0f));

        // Physical Resonator
        auto physResPrefix = slotPrefix + "PHYSRES_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(physResPrefix + "MODEL", "Model", juce::StringArray{ "Modal", "Sympathetic", "String" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "TUNE", "Tune", juce::NormalisableRange<float>(20.0f, 5000.0f, 0.0f, 0.25f), 220.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "STRUCTURE", "Structure", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "BRIGHTNESS", "Brightness", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "DAMPING", "Damping", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "POSITION", "Position", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "EXCITE_TYPE", "Excite Type", 0.0f, 1.0f, 0.8f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "SENSITIVITY", "Sensitivity", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "MIX", "Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(physResPrefix + "NOISE_TYPE", "Noise Type", juce::StringArray{ "White", "Pink" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "ATTACK", "Attack", juce::NormalisableRange<float>(0.001f, 1.0f, 0.0f, 0.3f), 0.001f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "DECAY", "Decay", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.3f), 0.05f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "SUSTAIN", "Sustain", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(physResPrefix + "RELEASE", "Release", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.3f), 0.01f));

        // Spectral Animator
        auto specAnimPrefix = slotPrefix + "SPECANIM_";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(specAnimPrefix + "MODE", "Mode", juce::StringArray{ "Pitch", "Formant" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "PITCH", "Pitch (Hz)", juce::NormalisableRange<float>(50.0f, 2000.0f, 0.1f, 0.3f), 440.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "FORMANT_X", "Formant X (Back/Front)", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "FORMANT_Y", "Formant Y (Close/Open)", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "MORPH", "Morph", 0.0f, 1.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(specAnimPrefix + "TRANSIENT_PRESERVE", "Transients", 0.0f, 1.0f, 0.8f));

        // Helical Delay
        auto helicalPrefix = slotPrefix + "HELICAL_";
        params.push_back(std::make_unique<juce::AudioParameterFloat>(helicalPrefix + "TIME", "Time", juce::NormalisableRange<float>(10.0f, 2000.0f, 0.1f, 0.3f), 400.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(helicalPrefix + "PITCH", "Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(helicalPrefix + "FEEDBACK", "Feedback", juce::NormalisableRange<float>(0.0f, 1.05f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(helicalPrefix + "DEGRADE", "Degrade", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(helicalPrefix + "TEXTURE", "Texture", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.1f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(helicalPrefix + "MIX", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

        // Chrono-Verb
        auto chronoPrefix = slotPrefix + "CHRONO_";
        params.push_back(std::make_unique<juce::AudioParameterFloat>(chronoPrefix + "SIZE", "Size", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(chronoPrefix + "DECAY", "Decay", 0.0f, 1.1f, 0.75f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(chronoPrefix + "DIFFUSION", "Diffusion", 0.0f, 1.0f, 0.8f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(chronoPrefix + "DAMPING", "Damping", juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.3f), 4000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(chronoPrefix + "MODULATION", "Modulation", 0.0f, 1.0f, 0.2f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(chronoPrefix + "BALANCE", "Balance", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(chronoPrefix + "MIX", "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(chronoPrefix + "FREEZE", "Freeze", false));


        // Tectonic Delay
        auto tectonicPrefix = slotPrefix + "TECTONIC_";
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "LOW_TIME", "Low Time (ms)", juce::NormalisableRange<float>(1.0f, 4000.0f, 0.1f, 0.5f), 100.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "MID_TIME", "Mid Time (ms)", juce::NormalisableRange<float>(1.0f, 4000.0f, 0.1f, 0.5f), 200.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "HIGH_TIME", "High Time (ms)", juce::NormalisableRange<float>(1.0f, 4000.0f, 0.1f, 0.5f), 150.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "FEEDBACK", "Feedback", juce::NormalisableRange<float>(0.0f, 1.1f, 0.001f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "LOMID_CROSS", "Low/Mid Cross (Hz)", juce::NormalisableRange<float>(100.0f, 8000.0f, 1.0f, 0.3f), 400.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "MIDHIGH_CROSS", "Mid/High Cross (Hz)", juce::NormalisableRange<float>(100.0f, 8000.0f, 1.0f, 0.3f), 2500.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "DECAY_DRIVE", "Decay Drive (dB)", 0.0f, 24.0f, 6.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "DECAY_TEXTURE", "Decay Texture", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "DECAY_DENSITY", "Decay Density", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "DECAY_PITCH", "Decay Pitch (st)", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(tectonicPrefix + "LINK", "Link", true));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(tectonicPrefix + "MIX", "Mix", 0.0f, 1.0f, 0.5f));
    }

    // Global Parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>("OVERSAMPLING_ALGO", "OS Algorithm", juce::StringArray{ "Live (IIR)", "HQ (FIR)", "Deluxe (FIR)" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("OVERSAMPLING_RATE", "OS Rate", juce::StringArray{ "1x (Off)", "2x", "4x", "8x", "16x" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MASTER_MIX", "Master Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("INPUT_GAIN", "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("OUTPUT_GAIN", "Output Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("SAG_ENABLE", "Auto-Gain", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SAG_RESPONSE", "Response (ms)", juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.5f), 50.0f));

    return { params.begin(), params.end() };
}

void ModularMultiFxAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty("visibleSlotCount", getVisibleSlotCount(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void ModularMultiFxAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        setVisibleSlotCount(apvts.state.getProperty("visibleSlotCount", 8));
    }
}

bool ModularMultiFxAudioProcessor::checkForChromaTapeUsage() const
{
    int numVisible = getVisibleSlotCount();
    for (int i = 0; i < numVisible; ++i)
    {
        auto choiceParamId = "SLOT_" + juce::String(i + 1) + "_CHOICE";
        if (auto* p = apvts.getRawParameterValue(choiceParamId))
            if (static_cast<int>(p->load()) == 7) // 7 is ChromaTape
                return true;
    }
    return false;
}

void ModularMultiFxAudioProcessor::updateOversamplingConfiguration()
{
    auto newAlgo = pendingOSAlgo.load();
    auto newRate = pendingOSRate.load();
    bool lock = false;

    if (isNonRealtime())
    {
        newAlgo = OversamplingAlgorithm::Deluxe;
        newRate = OversamplingRate::x8;
    }
    else if (checkForChromaTapeUsage())
    {
        lock = true;
        if (newRate > OversamplingRate::x2)
            newRate = OversamplingRate::x2;
    }

    bool changed = (effectiveOSAlgo.load() != newAlgo || effectiveOSRate.load() != newRate);
    bool lockChanged = (oversamplingLockActive.load() != lock);

    if (changed)
    {
        effectiveOSAlgo.store(newAlgo);
        effectiveOSRate.store(newRate);
        isGraphDirty.store(true);
    }
    if (lockChanged)
    {
        oversamplingLockActive.store(lock);
        juce::MessageManager::callAsync([this] { if (this) osLockChangeBroadcaster.sendChangeMessage(); });
    }
}

void ModularMultiFxAudioProcessor::initiateGraphUpdate()
{
    if (previousContext)
    {
        if (previousContext->graph) previousContext->graph->releaseResources();
        previousContext.reset();
    }
    previousContext = std::move(activeContext);
    activeContext = std::make_unique<ProcessingContextWrapper>();

    auto algo = effectiveOSAlgo.load();
    auto rate = effectiveOSRate.load();
    int channels = currentOSChannels.load();
    if (channels == 0) channels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (channels == 0) channels = 2;

    if (channels > 0)
        activeContext->oversampler = createOversamplingEngine(rate, algo, channels);

    if (activeContext->oversampler)
    {
        if (preparedMaxBlockSize > 0)
            activeContext->oversampler->initProcessing(preparedMaxBlockSize);
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
        if (activeContext->graph)
            totalLatency += activeContext->graph->getLatencySamples();

        setLatencySamples(totalLatency);
    }
}

bool ModularMultiFxAudioProcessor::updateGraph()
{
    if (preparedSampleRate <= 0 || preparedMaxBlockSize <= 0) return false;
    if (!activeContext || !activeContext->graph) return false;

    int channels = currentOSChannels.load();
    if (channels == 0)
    {
        channels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
        if (channels == 0) channels = 2;
    }

    activeContext->graph->clear();

    double graphSR = preparedSampleRate;
    int graphBS = preparedMaxBlockSize;
    if (activeContext->oversampler)
    {
        graphSR *= activeContext->oversampler->getOversamplingFactor();
        graphBS = (int)((double)graphBS * activeContext->oversampler->getOversamplingFactor());
    }

    if (channels > 0 && activeContext->oversampler)
        activeContext->oversampledGraphBuffer.setSize(channels, graphBS);
    else
        activeContext->oversampledGraphBuffer.setSize(0, 0);

    activeContext->graph->setPlayConfigDetails(channels, channels, graphSR, graphBS);

    inputNode = activeContext->graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    outputNode = activeContext->graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    auto connect = [&](juce::AudioProcessorGraph::Node* src, juce::AudioProcessorGraph::Node* dst)
        {
            if (!src || !dst) return;
            for (int ch = 0; ch < channels; ++ch)
                if (activeContext->graph->canConnect({ { src->nodeID, ch }, { dst->nodeID, ch } }))
                    activeContext->graph->addConnection({ { src->nodeID, ch }, { dst->nodeID, ch } });
        };

    juce::AudioProcessorGraph::Node* last = inputNode.get();
    bool added = false;
    int numVisible = getVisibleSlotCount();
    const int numCols = 4;
    int slotsConsumed = 0;

    for (int i = 0; i < numVisible; i += slotsConsumed)
    {
        slotsConsumed = 1;
        auto choiceParamId = "SLOT_" + juce::String(i + 1) + "_CHOICE";
        int choice = 0;
        if (auto* p = apvts.getRawParameterValue(choiceParamId)) choice = (int)p->load();
        if (choice == 7) slotsConsumed = 3; // ChromaTape spans multiple slots
        int currentCol = i % numCols;
        if (currentCol + slotsConsumed > numCols) slotsConsumed = std::max(1, numCols - currentCol);
        if (i + slotsConsumed > numVisible) slotsConsumed = numVisible - i;
        if (slotsConsumed <= 0) break;
        if (choice > 0)
        {
            fxSlotNodes[i] = activeContext->graph->addNode(createProcessorForChoice(choice, i));
            if (fxSlotNodes[i]) { connect(last, fxSlotNodes[i].get()); last = fxSlotNodes[i].get(); added = true; }
            for (int j = 1; j < slotsConsumed; ++j) if (i + j < maxSlots) fxSlotNodes[i + j] = nullptr;
        }
        else
        {
            for (int j = 0; j < slotsConsumed; ++j) if (i + j < maxSlots) fxSlotNodes[i + j] = nullptr;
        }
    }

    if (!added)
    {
        auto wire = activeContext->graph->addNode(std::make_unique<PassThroughProcessor>());
        if (wire) { connect(last, wire.get()); last = wire.get(); }
    }

    connect(last, outputNode.get());
    activeContext->graph->prepareToPlay(graphSR, graphBS);
    for (auto node : activeContext->graph->getNodes()) if (node && node->getProcessor()) node->getProcessor()->enableAllBuses();
    return true;
}

std::unique_ptr<juce::AudioProcessor> ModularMultiFxAudioProcessor::createProcessorForChoice(int choice, int slotIndex)
{
    switch (choice)
    {
    case 1:  return std::make_unique<DistortionProcessor>(apvts, slotIndex);
    case 2:  return std::make_unique<FilterProcessor>(apvts, slotIndex);
    case 3:  return std::make_unique<ModulationProcessor>(apvts, slotIndex);
    case 4:  return std::make_unique<AdvancedDelayProcessor>(apvts, slotIndex);
    case 5:  return std::make_unique<ReverbProcessor>(apvts, slotIndex);
    case 6:  return std::make_unique<AdvancedCompressorProcessor>(apvts, slotIndex);
    case 7:  return std::make_unique<ChromaTapeProcessor>(apvts, slotIndex);
    case 8:  return std::make_unique<MorphoCompProcessor>(apvts, slotIndex);
    case 9:  return std::make_unique<PhysicalResonatorProcessor>(apvts, slotIndex);
    case 10: return std::make_unique<SpectralAnimatorProcessor>(apvts, slotIndex);
    case 11: return std::make_unique<HelicalDelayProcessor>(apvts, slotIndex);
    case 12: return std::make_unique<ChronoVerbProcessor>(apvts, slotIndex);
    case 13: return std::make_unique<TectonicDelayProcessor>(apvts, slotIndex);
    default: return nullptr;
    }
}

void ModularMultiFxAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID.endsWith("_CHOICE") && parameterID.startsWith("SLOT_"))
    {
        isGraphDirty.store(true);
        editorResizeBroadcaster.sendChangeMessage();
    }
    if (parameterID == "OVERSAMPLING_ALGO")
        pendingOSAlgo.store(static_cast<OversamplingAlgorithm>((int)newValue));
    else if (parameterID == "OVERSAMPLING_RATE")
        pendingOSRate.store(static_cast<OversamplingRate>((int)newValue));
    if (parameterID.startsWith("SAG_"))
        updateSmartAutoGainParameters();
    if (parameterID == "INPUT_GAIN" || parameterID == "OUTPUT_GAIN")
        updateGainStages();
}

void ModularMultiFxAudioProcessor::updateSmartAutoGainParameters()
{
    if (auto* pEnable = apvts.getRawParameterValue("SAG_ENABLE"))
        smartAutoGain.setEnabled(pEnable->load() > 0.5f);
    if (auto* pResp = apvts.getRawParameterValue("SAG_RESPONSE"))
        smartAutoGain.setResponseTime(pResp->load());
}

std::unique_ptr<juce::dsp::Oversampling<float>> ModularMultiFxAudioProcessor::createOversamplingEngine(OversamplingRate rate, OversamplingAlgorithm algo, int numChannels)
{
    if (numChannels <= 0 || rate == OversamplingRate::x1) return nullptr;
    int stages = 0;
    switch (rate)
    {
    case OversamplingRate::x2: stages = 1; break;
    case OversamplingRate::x4: stages = 2; break;
    case OversamplingRate::x8: stages = 3; break;
    case OversamplingRate::x16: stages = 4; break;
    default: return nullptr;
    }
    juce::dsp::Oversampling<float>::FilterType fType;
    bool linearPhase = true;
    switch (algo)
    {
    case OversamplingAlgorithm::Live: fType = juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR; linearPhase = false; break;
    case OversamplingAlgorithm::HQ:
    case OversamplingAlgorithm::Deluxe: fType = juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple; linearPhase = true; break;
    }
    return std::make_unique<juce::dsp::Oversampling<float>>((size_t)numChannels, stages, fType, linearPhase);
}

void ModularMultiFxAudioProcessor::setVisibleSlotCount(int n)
{
    int clamped = juce::jlimit(0, maxSlots, n);
    if (clamped == visibleSlotCountInt) return;
    visibleSlotCountInt = clamped;

    // This value object is now just for state saving/loading.
    // The UI will query getVisibleSlotCount().
    apvts.state.setProperty("visibleSlotCount", clamped, nullptr);

    editorResizeBroadcaster.sendChangeMessage();
}

double ModularMultiFxAudioProcessor::getTailLengthSeconds() const
{
    if (activeContext && activeContext->graph)
    {
        double tail = activeContext->graph->getTailLengthSeconds();
        if (activeContext->oversampler && activeContext->oversampler->getOversamplingFactor() > 1)
        {
            tail /= activeContext->oversampler->getOversamplingFactor();
        }
        return tail + 0.1; // Add a small safety margin
    }
    return 0.1;
}

juce::AudioProcessorEditor* ModularMultiFxAudioProcessor::createEditor()
{
    return new ModularMultiFxAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModularMultiFxAudioProcessor();
}