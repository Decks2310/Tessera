//================================================================================
// File: FX_Modules/SpectralAnimatorProcessor.cpp
//================================================================================
#include "SpectralAnimatorProcessor.h"

SpectralAnimatorProcessor::SpectralAnimatorProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    // Define Parameter IDs (Using a distinct prefix SPECANIM_)
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_SPECANIM_";
    modeParamId = slotPrefix + "MODE";
    pitchParamId = slotPrefix + "PITCH";
    formantXParamId = slotPrefix + "FORMANT_X";
    formantYParamId = slotPrefix + "FORMANT_Y";
    morphParamId = slotPrefix + "MORPH";
    transientParamId = slotPrefix + "TRANSIENT_PRESERVE";
}

void SpectralAnimatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };
    engine.prepare(spec);

    // Report the latency introduced by the STFT process (equal to the FFT Size).
    setLatencySamples(SpectralAnimatorEngine::FFT_SIZE);
}

void SpectralAnimatorProcessor::releaseResources()
{
    engine.reset();
}

void SpectralAnimatorProcessor::reset()
{
    engine.reset();
}

void SpectralAnimatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // P1 Boilerplate
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update Parameters (Ensure parameters exist before accessing)
    if (!mainApvts.getRawParameterValue(modeParamId) || !mainApvts.getRawParameterValue(pitchParamId) ||
        !mainApvts.getRawParameterValue(formantXParamId) || !mainApvts.getRawParameterValue(formantYParamId) ||
        !mainApvts.getRawParameterValue(morphParamId) || !mainApvts.getRawParameterValue(transientParamId))
    {
        return;
    }

    // Load parameters and update the engine
    auto mode = static_cast<SpectralAnimatorEngine::Mode>(static_cast<int>(mainApvts.getRawParameterValue(modeParamId)->load()));
    float pitch = mainApvts.getRawParameterValue(pitchParamId)->load();
    float formantX = mainApvts.getRawParameterValue(formantXParamId)->load();
    float formantY = mainApvts.getRawParameterValue(formantYParamId)->load();
    float morph = mainApvts.getRawParameterValue(morphParamId)->load();
    float transient = mainApvts.getRawParameterValue(transientParamId)->load();

    engine.setMode(mode);
    engine.setPitch(pitch);
    engine.setFormant(formantX, formantY);
    engine.setMorph(morph);
    engine.setTransientPreservation(transient);

    // Process audio through the engine
    engine.process(buffer);
}