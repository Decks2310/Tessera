//================================================================================
// File: FX_Modules/DelayProcessor.cpp
//================================================================================
#include "DelayProcessor.h"

DelayProcessor::DelayProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_";
    typeParamId = slotPrefix + "DELAY_TYPE";
    timeParamId = slotPrefix + "DELAY_TIME";
    feedbackParamId = slotPrefix + "DELAY_FEEDBACK";
    mixParamId = slotPrefix + "DELAY_MIX";
    dampingParamId = slotPrefix + "DELAY_DAMPING";
}

void DelayProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };
    delayLine.prepare(spec);
    delayLine.setMaximumDelayInSamples(static_cast<int>(sampleRate * 2.0));
    feedbackFilter.prepare(spec);
    feedbackFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    reset();
}

void DelayProcessor::releaseResources()
{
    reset();
}

void DelayProcessor::reset()
{
    delayLine.reset();
    feedbackFilter.reset();
}


// Replace processBlock entirely:
void DelayProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    // FIX P1: Boilerplate
    juce::ScopedNoDenormals noDenormals;
    // FIX C4100
    juce::ignoreUnused(midiMessages);
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    // Get parameters
    auto type = static_cast<int>(mainApvts.getRawParameterValue(typeParamId)->load());
    float time = mainApvts.getRawParameterValue(timeParamId)->load();
    float feedback = mainApvts.getRawParameterValue(feedbackParamId)->load();
    float mix = mainApvts.getRawParameterValue(mixParamId)->load();
    float damping = mainApvts.getRawParameterValue(dampingParamId)->load();

    float delaySamples = (float)(getSampleRate() * time / 1000.0);
    feedbackFilter.setCutoffFrequency(damping);
    // FIX P3: Corrected feedback loop
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        // ✅ FINAL FIX: The filter was correctly prepared for stereo, so we just need to process each channel.
        // The previous check was incorrect because StateVariableTPTFilter *does not* have getNumChannels().
        auto* channelData = buffer.getWritePointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float inputSample = channelData[i];
            // 1. Get the delayed sample
            float delayedSample = delayLine.popSample(channel, delaySamples, true);
            // 2. Apply damping filter to the delayed signal
            float filteredDelayedSample = feedbackFilter.processSample(channel, delayedSample);
            // 3. Apply saturation if analog mode
            if (type == 1) // Analog BBD
            {
                filteredDelayedSample = std::tanh(filteredDelayedSample);
            }

            // 4. Calculate feedback amount
            float feedbackSample = filteredDelayedSample * feedback;
            // 5. Calculate input to the delay line
            float inputToDelay = inputSample + feedbackSample;
            // 6. Push into the delay line
            delayLine.pushSample(channel, inputToDelay);
            // 7. Calculate output mix (Use the processed signal)
            channelData[i] = (inputSample * (1.0f - mix)) + (filteredDelayedSample * mix);
        }
    }
}