#include "TectonicDelayProcessor.h"

//==============================================================================
// CrossoverNetwork Implementation
//==============================================================================
void TectonicDelayProcessor::CrossoverNetwork::prepare(const juce::dsp::ProcessSpec& spec)
{
    lowMidLowpass.prepare(spec); lowMidLowpass.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowMidHighpass.prepare(spec); lowMidHighpass.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    midHighLowpass.prepare(spec); midHighLowpass.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    midHighHighpass.prepare(spec); midHighHighpass.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    int ch = (int)spec.numChannels; int bs = (int)spec.maximumBlockSize;
    lowBand.setSize(ch, bs); midBand.setSize(ch, bs); highBand.setSize(ch, bs);
}

void TectonicDelayProcessor::CrossoverNetwork::reset()
{
    lowMidLowpass.reset(); lowMidHighpass.reset(); midHighLowpass.reset(); midHighHighpass.reset();
}

void TectonicDelayProcessor::CrossoverNetwork::setCrossoverFrequencies(float lowMid, float midHigh)
{
    midHigh = juce::jmax(lowMid + 20.0f, midHigh);
    lowMidLowpass.setCutoffFrequency(lowMid);
    lowMidHighpass.setCutoffFrequency(lowMid);
    midHighLowpass.setCutoffFrequency(midHigh);
    midHighHighpass.setCutoffFrequency(midHigh);
}

void TectonicDelayProcessor::CrossoverNetwork::processBlock(juce::AudioBuffer<float>& buffer)
{
    int ch = buffer.getNumChannels(); int ns = buffer.getNumSamples();
    lowBand.setSize(ch, ns, false, false, true);
    midBand.setSize(ch, ns, false, false, true);
    highBand.setSize(ch, ns, false, false, true);

    // Low band
    lowBand.makeCopyOf(buffer);
    {
        juce::dsp::AudioBlock<float> blk(lowBand);
        juce::dsp::ProcessContextReplacing<float> ctx(blk);
        lowMidLowpass.process(ctx);
    }
    // High band (start from input)
    highBand.makeCopyOf(buffer);
    {
        juce::dsp::AudioBlock<float> blk(highBand);
        juce::dsp::ProcessContextReplacing<float> ctx(blk);
        lowMidHighpass.process(ctx); // now > lowMid
        // Copy to mid and carve
        midBand.makeCopyOf(highBand);
        juce::dsp::AudioBlock<float> midBlk(midBand);
        juce::dsp::ProcessContextReplacing<float> midCtx(midBlk);
        midHighLowpass.process(midCtx); // band-pass region
        // Finish high as > midHigh
        lowMidHighpass.reset(); // ensure phase continuity not broken for future blocks
        midHighHighpass.process(ctx);
    }
}

//==============================================================================
// Constructor (single definition)
//==============================================================================
TectonicDelayProcessor::TectonicDelayProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mainApvts(apvts)
{
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_TECTONIC_";
    lowTimeParamId          = slotPrefix + "LOW_TIME";
    midTimeParamId          = slotPrefix + "MID_TIME";
    highTimeParamId         = slotPrefix + "HIGH_TIME";
    feedbackParamId         = slotPrefix + "FEEDBACK";
    lowMidCrossoverParamId  = slotPrefix + "LOMID_CROSS";
    midHighCrossoverParamId = slotPrefix + "MIDHIGH_CROSS";
    decayDriveParamId       = slotPrefix + "DECAY_DRIVE";
    decayTextureParamId     = slotPrefix + "DECAY_TEXTURE";
    decayDensityParamId     = slotPrefix + "DECAY_DENSITY";
    decayPitchParamId       = slotPrefix + "DECAY_PITCH";
    linkParamId             = slotPrefix + "LINK";
    mixParamId              = slotPrefix + "MIX";
}

//==============================================================================
void TectonicDelayProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    this->maxBlockSize = samplesPerBlock;

    int channels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (channels == 0) channels = 2;
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)channels };

    crossover.prepare(spec);
    crossover.setCrossoverFrequencies(params.lowMidCrossover, params.midHighCrossover);

    for (auto& band : delayBands)
        band.prepare(spec, sampleRate, samplesPerBlock);

    dryBuffer.setSize(channels, samplesPerBlock);
    wetBuffer.setSize(channels, samplesPerBlock);

    double smooth = 0.05; // 50ms
    smoothedFeedback.reset(sampleRate, smooth);
    smoothedDecayDrive.reset(sampleRate, smooth);
    smoothedDecayTexture.reset(sampleRate, smooth);
    smoothedDecayDensity.reset(sampleRate, smooth);
    smoothedDecayPitch.reset(sampleRate, smooth);
    smoothedMix.reset(sampleRate, smooth);

    updateParameters();

    smoothedFeedback.setCurrentAndTargetValue(params.feedback);
    smoothedDecayDrive.setCurrentAndTargetValue(params.decayDrive);
    smoothedDecayTexture.setCurrentAndTargetValue(params.decayTexture);
    smoothedDecayDensity.setCurrentAndTargetValue(params.decayDensity);
    smoothedDecayPitch.setCurrentAndTargetValue(params.decayPitch);
    smoothedMix.setCurrentAndTargetValue(params.mix);
}

void TectonicDelayProcessor::releaseResources()
{
    dryBuffer.setSize(0, 0);
    wetBuffer.setSize(0, 0);
}

void TectonicDelayProcessor::reset()
{
    crossover.reset();
    for (auto& b : delayBands) b.reset();
    updateParameters();
}

//==============================================================================
void TectonicDelayProcessor::updateParameters()
{
    auto getParam = [&](const juce::String& id, float def) {
        if (auto* p = mainApvts.getRawParameterValue(id)) return p->load();
        return def;
    };
    params.lowTime          = getParam(lowTimeParamId, 100.0f);
    params.midTime          = getParam(midTimeParamId, 200.0f);
    params.highTime         = getParam(highTimeParamId,150.0f);
    params.feedback         = getParam(feedbackParamId, 0.3f);
    params.lowMidCrossover  = getParam(lowMidCrossoverParamId, 400.0f);
    params.midHighCrossover = getParam(midHighCrossoverParamId, 2500.0f);
    params.decayDrive       = getParam(decayDriveParamId, 6.0f);
    params.decayTexture     = getParam(decayTextureParamId, 0.5f);
    params.decayDensity     = getParam(decayDensityParamId, 0.5f);
    params.decayPitch       = getParam(decayPitchParamId, 0.0f);
    params.linked           = getParam(linkParamId, 1.0f) > 0.5f;
    params.mix              = getParam(mixParamId, 0.5f);

    crossover.setCrossoverFrequencies(params.lowMidCrossover, params.midHighCrossover);

    smoothedFeedback.setTargetValue(params.feedback);
    smoothedDecayDrive.setTargetValue(params.decayDrive);
    smoothedDecayTexture.setTargetValue(params.decayTexture);
    smoothedDecayDensity.setTargetValue(params.decayDensity);
    smoothedDecayPitch.setTargetValue(params.decayPitch);
    smoothedMix.setTargetValue(params.mix);
}

//==============================================================================
void TectonicDelayProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    int totalIn = getTotalNumInputChannels();
    int totalOut = getTotalNumOutputChannels();
    int numSamples = buffer.getNumSamples();
    int channels = juce::jmax(totalIn, totalOut);
    if (channels == 0) return;

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    updateParameters();

    dryBuffer.setSize(channels, numSamples, false, false, true);
    dryBuffer.makeCopyOf(buffer);
    wetBuffer.setSize(channels, numSamples, false, false, true);
    wetBuffer.clear();

    // 1. Split
    crossover.processBlock(buffer);
    auto& lowBand  = crossover.getLowBand();
    auto& midBand  = crossover.getMidBand();
    auto& highBand = crossover.getHighBand();

    std::array<juce::AudioBuffer<float>*, 3> bands{ &lowBand, &midBand, &highBand };
    std::array<float, 3> times{ params.lowTime, params.midTime, params.highTime };

    float fb      = smoothedFeedback.getNextValue();
    float drive   = smoothedDecayDrive.getNextValue();
    float texture = smoothedDecayTexture.getNextValue();
    float density = smoothedDecayDensity.getNextValue();
    float pitch   = smoothedDecayPitch.getNextValue();

    for (int b = 0; b < 3; ++b)
    {
        delayBands[b].processBlock(*bands[b], times[b], fb, drive, texture, density, pitch);
    }

    // 3. Sum
    for (int b = 0; b < 3; ++b)
    {
        for (int ch = 0; ch < channels; ++ch)
            if (ch < bands[b]->getNumChannels())
                wetBuffer.addFrom(ch, 0, *bands[b], ch, 0, numSamples);
    }

    // 4. Mix
    float mix = smoothedMix.getNextValue();
    float dryGain = 1.0f - mix;
    float wetGain = mix;
    for (int ch = 0; ch < totalOut; ++ch)
    {
        float* out = buffer.getWritePointer(ch);
        const float* dry = dryBuffer.getReadPointer(juce::jmin(ch, dryBuffer.getNumChannels()-1));
        const float* wet = wetBuffer.getReadPointer(juce::jmin(ch, wetBuffer.getNumChannels()-1));
        for (int i = 0; i < numSamples; ++i)
            out[i] = dry[i] * dryGain + wet[i] * wetGain;
    }
}