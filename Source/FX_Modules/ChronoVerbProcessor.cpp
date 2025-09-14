/*
  ==============================================================================

    ChronoVerbProcessor.cpp - Clean Implementation
    Hybrid reverb processor following original elegant design
    Path A: Early Reflections (Multi-tap delay)
    Path B: Late Reflections (Spectral diffusion)
    Simple feedback path with damping
    Author: Fixed by AI Assistant

  ==============================================================================
*/

#include "ChronoVerbProcessor.h"

//==============================================================================
// Early Reflections Generator Implementation
//==============================================================================
void ChronoVerbProcessor::EarlyReflectionsGenerator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = (int)spec.numChannels;
    
    // Prepare multi-tap delay buffer with full capacity
    int maxDelaySamples = (int)(sampleRate * 2.0f); // 2 seconds maximum
    multiTapDelay.prepare(spec, maxDelaySamples);
    
    // Define the 8 taps with musical, non-harmonic ratios
    tapDefinitions = {{
        {0.029f, 0.95f, -0.85f},  // Early left reflection
        {0.051f, 0.90f, 0.78f},   // Early right reflection
        {0.083f, 0.85f, -0.62f},  // Secondary left
        {0.118f, 0.80f, 0.55f},   // Secondary right
        {0.149f, 0.75f, -0.41f},  // Tertiary left
        {0.182f, 0.70f, 0.33f},   // Tertiary right
        {0.214f, 0.65f, -0.18f},  // Late left
        {0.248f, 0.60f, 0.10f}    // Late right
    }};
    
    // Prepare modulation LFO
    modLFO.prepare(spec);
    modLFO.setWaveform(DSPUtils::LFO::Waveform::Sine);
    modLFO.setFrequency(0.3f); // Musical modulation rate
    
    reset();
}

void ChronoVerbProcessor::EarlyReflectionsGenerator::reset()
{
    multiTapDelay.reset();
    modLFO.reset();
}

void ChronoVerbProcessor::EarlyReflectionsGenerator::processBlock(const juce::AudioBuffer<float>& input, 
                                                                 juce::AudioBuffer<float>& output, 
                                                                 float size, float modulation)
{
    int numSamples = input.getNumSamples();
    output.clear();
    
    // Write input to delay buffer - every sample processed
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels && ch < input.getNumChannels(); ++ch)
        {
            multiTapDelay.writeSample(ch, input.getSample(ch, i));
        }
        multiTapDelay.advanceWritePosition();
    }
    
    // Process each sample with all taps
    for (int i = 0; i < numSamples; ++i)
    {
        float modLFOValue = modLFO.getNextBipolar() * modulation * 0.005f; // Proper modulation depth
        
        for (int ch = 0; ch < numChannels && ch < output.getNumChannels(); ++ch)
        {
            float erOutput = 0.0f;
            
            // Sum all 8 taps - no skipping
            for (const auto& tap : tapDefinitions)
            {
                float baseDelayTime = tap.delayRatio * size;
                float modulatedDelayTime = baseDelayTime * (1.0f + modLFOValue);
                float delaySamples = modulatedDelayTime * (float)sampleRate;
                
                float readPos = (float)multiTapDelay.getWritePosition() - delaySamples - (float)i;
                float tapSample = multiTapDelay.read(ch, readPos);
                
                // Apply gain and proper stereo panning
                tapSample *= tap.gain;
                if (numChannels == 2)
                {
                    float pan = tap.pan;
                    if (ch == 0) // Left channel
                        tapSample *= (1.0f - pan) * 0.5f + 0.5f;
                    else // Right channel
                        tapSample *= (1.0f + pan) * 0.5f + 0.5f;
                }
                
                erOutput += tapSample;
            }
            
            output.setSample(ch, i, erOutput * 0.7f); // Proper level scaling
        }
    }
}

//==============================================================================
// Late Reflections Generator Implementation
//==============================================================================
void ChronoVerbProcessor::LateReflectionsGenerator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = (int)spec.numChannels;
    
    // Simple preparation - just the diffuser
    diffuser.prepare(spec);
    
    reset();
}

void ChronoVerbProcessor::LateReflectionsGenerator::reset()
{
    diffuser.reset();
}

void ChronoVerbProcessor::LateReflectionsGenerator::processBlock(const juce::AudioBuffer<float>& input, 
                                                                juce::AudioBuffer<float>& output, 
                                                                float diffusion)
{
    output.makeCopyOf(input);
    
    // Clean spectral diffusion - no unnecessary complexity
    diffuser.process(output, diffusion);
}

//==============================================================================
// Feedback Path Implementation
//==============================================================================
void ChronoVerbProcessor::FeedbackPath::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = (int)spec.numChannels;
    
    // Simple damping filter
    dampingFilter.prepare(spec);
    dampingFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    dampingFilter.setResonance(0.707f);
    
    reset();
}

void ChronoVerbProcessor::FeedbackPath::reset()
{
    dampingFilter.reset();
}

void ChronoVerbProcessor::FeedbackPath::processBlock(juce::AudioBuffer<float>& buffer, float damping)
{
    // Update filter frequency
    float dampingFreq = juce::jmap(damping, 200.0f, 20000.0f);
    dampingFilter.setCutoffFrequency(dampingFreq);
    
    // Apply damping filter
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    dampingFilter.process(context);
    
    // Gentle saturation to prevent runaway feedback
    for (int ch = 0; ch < numChannels && ch < buffer.getNumChannels(); ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            channelData[i] = DSPUtils::fastTanh(channelData[i] * 0.95f);
        }
    }
}

//==============================================================================
// Main ChronoVerbProcessor Implementation
//==============================================================================
ChronoVerbProcessor::ChronoVerbProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_CHRONO_";
    
    sizeParamId = slotPrefix + "SIZE";
    decayParamId = slotPrefix + "DECAY";
    balanceParamId = slotPrefix + "BALANCE";
    freezeParamId = slotPrefix + "FREEZE";
    diffusionParamId = slotPrefix + "DIFFUSION";
    dampingParamId = slotPrefix + "DAMPING";
    modulationParamId = slotPrefix + "MODULATION";
    mixParamId = slotPrefix + "MIX";
}

void ChronoVerbProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    this->maxBlockSize = samplesPerBlock;
    
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, 
                                (juce::uint32)std::max(getTotalNumInputChannels(), getTotalNumOutputChannels()) };
    if (spec.numChannels == 0) spec.numChannels = 2;
    
    // Prepare DSP modules
    earlyReflections.prepare(spec);
    lateReflections.prepare(spec);
    feedbackPath.prepare(spec);
    
    // Prepare delay lines
    preDelay.prepare(spec);
    preDelay.setMaximumDelayInSamples((int)(sampleRate * 0.2)); // 200ms max pre-delay
    
    latencyCompensationDelay.prepare(spec);
    int lrLatency = lateReflections.getLatencySamples();
    latencyCompensationDelay.setMaximumDelayInSamples(lrLatency + 64);
    latencyCompensationDelay.setDelay((float)lrLatency);
    setLatencySamples(lrLatency);
    
    // Prepare buffers
    int numChannels = (int)spec.numChannels;
    preDelayBuffer.setSize(numChannels, samplesPerBlock);
    earlyReflectionsBuffer.setSize(numChannels, samplesPerBlock);
    lateReflectionsBuffer.setSize(numChannels, samplesPerBlock);
    wetBuffer.setSize(numChannels, samplesPerBlock);
    feedbackBuffer.setSize(numChannels, samplesPerBlock);
    
    // Initialize smoothed parameters with proper smoothing time
    double smoothTime = 0.08; // 80ms for smooth, musical parameter changes
    smSize.reset(sampleRate, smoothTime);
    smDecay.reset(sampleRate, smoothTime);
    smBalance.reset(sampleRate, smoothTime);
    smDiffusion.reset(sampleRate, smoothTime);
    smDamping.reset(sampleRate, smoothTime);
    smModulation.reset(sampleRate, smoothTime);
    smMix.reset(sampleRate, smoothTime);
    
    reset();
}

void ChronoVerbProcessor::releaseResources()
{
    reset();
}

void ChronoVerbProcessor::reset()
{
    earlyReflections.reset();
    lateReflections.reset();
    feedbackPath.reset();
    preDelay.reset();
    latencyCompensationDelay.reset();
    
    // Clear buffers
    preDelayBuffer.clear();
    earlyReflectionsBuffer.clear();
    lateReflectionsBuffer.clear();
    wetBuffer.clear();
    feedbackBuffer.clear();
    
    updateParameters();
    
    // Set initial smoothed values
    smSize.setCurrentAndTargetValue(params.size);
    smDecay.setCurrentAndTargetValue(params.decay);
    smBalance.setCurrentAndTargetValue(params.balance);
    smDiffusion.setCurrentAndTargetValue(params.diffusion);
    smDamping.setCurrentAndTargetValue(params.damping);
    smModulation.setCurrentAndTargetValue(params.modulation);
    smMix.setCurrentAndTargetValue(params.mix);
}

void ChronoVerbProcessor::updateParameters()
{
    auto getParam = [&](const juce::String& id, float defaultVal = 0.0f) {
        if (auto* p = mainApvts.getRawParameterValue(id)) 
            return p->load();
        return defaultVal;
    };
    
    params.size = getParam(sizeParamId, 0.5f);
    params.decay = getParam(decayParamId, 0.6f);
    params.balance = getParam(balanceParamId, 0.5f);
    params.freeze = getParam(freezeParamId, 0.0f) > 0.5f;
    params.diffusion = getParam(diffusionParamId, 0.7f);
    params.damping = getParam(dampingParamId, 0.5f);
    params.modulation = getParam(modulationParamId, 0.2f);
    params.mix = getParam(mixParamId, 0.5f);
    
    // Update smoothed parameter targets
    smSize.setTargetValue(params.size);
    smDecay.setTargetValue(params.decay);
    smBalance.setTargetValue(params.balance);
    smDiffusion.setTargetValue(params.diffusion);
    smDamping.setTargetValue(params.damping);
    smModulation.setTargetValue(params.modulation);
    smMix.setTargetValue(params.mix);
}

void ChronoVerbProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    
    auto totalIn = getTotalNumInputChannels();
    auto totalOut = getTotalNumOutputChannels();
    
    for (auto i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    int numSamples = buffer.getNumSamples();
    int numChannels = std::max(totalIn, totalOut);
    
    if (numChannels == 0) return;
    
    updateParameters();
    
    // Ensure buffers are the right size
    preDelayBuffer.setSize(numChannels, numSamples, false, false, true);
    earlyReflectionsBuffer.setSize(numChannels, numSamples, false, false, true);
    lateReflectionsBuffer.setSize(numChannels, numSamples, false, false, true);
    wetBuffer.setSize(numChannels, numSamples, false, false, true);
    
    preDelayBuffer.clear();
    earlyReflectionsBuffer.clear();
    lateReflectionsBuffer.clear();
    wetBuffer.clear();
    
    // Process pre-delay with feedback injection
    float decayGain = params.freeze ? 0.99f : smDecay.getNextValue() * 1.1f; // Allow >100% for infinite decay
    
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float inputSample = (ch < buffer.getNumChannels()) ? buffer.getSample(ch, i) : 0.0f;
            float feedbackSample = (ch < feedbackBuffer.getNumChannels()) ? 
                                   feedbackBuffer.getSample(ch, i) * decayGain : 0.0f;
            
            // Clean feedback injection
            float inputToEffect = params.freeze ? feedbackSample : (inputSample + feedbackSample);
            
            // Apply pre-delay
            preDelay.pushSample(ch, inputToEffect);
            float preDelayMs = smSize.getNextValue() * 100.0f; // 0-100ms pre-delay
            float preDelaySamples = preDelayMs * (float)sampleRate / 1000.0f;
            preDelay.setDelay(preDelaySamples);
            
            float preDelayedSample = preDelay.popSample(ch);
            preDelayBuffer.setSample(ch, i, preDelayedSample);
        }
    }
    
    // Path A: Process early reflections
    earlyReflections.processBlock(preDelayBuffer, earlyReflectionsBuffer, 
                                 smSize.getCurrentValue(),
                                 smModulation.getNextValue());
    
    // Path B: Process late reflections
    lateReflections.processBlock(preDelayBuffer, lateReflectionsBuffer, 
                                smDiffusion.getNextValue());
    
    // Apply latency compensation to early reflections
    juce::dsp::AudioBlock<float> erBlock(earlyReflectionsBuffer);
    juce::dsp::ProcessContextReplacing<float> context(erBlock);
    latencyCompensationDelay.process(context);
    
    // Balance early and late reflections - Original simple mix
    float balance = smBalance.getNextValue();
    float erGain = std::cos(balance * juce::MathConstants<float>::halfPi);
    float lrGain = std::sin(balance * juce::MathConstants<float>::halfPi);
    
    // Create wet signal
    wetBuffer.makeCopyOf(lateReflectionsBuffer);
    wetBuffer.applyGain(lrGain);
    for (int ch = 0; ch < numChannels; ++ch)
        wetBuffer.addFrom(ch, 0, earlyReflectionsBuffer, ch, 0, numSamples, erGain);
    
    // Process feedback path
    feedbackBuffer.makeCopyOf(wetBuffer);
    feedbackPath.processBlock(feedbackBuffer, smDamping.getNextValue());
    
    // Final wet/dry mix - Original simple blend
    float mix = smMix.getNextValue();
    float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
    float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
    
    for (int ch = 0; ch < totalOut; ++ch)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float dry = (ch < buffer.getNumChannels()) ? buffer.getSample(ch, i) : 0.0f;
            float wet = (ch < wetBuffer.getNumChannels()) ? wetBuffer.getSample(ch, i) : 0.0f;
            
            float output = dry * dryGain + wet * wetGain;
            buffer.setSample(ch, i, output);
        }
    }
}
