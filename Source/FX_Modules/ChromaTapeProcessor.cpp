//================================================================================
// File: FX_Modules/ChromaTapeProcessor.cpp
//================================================================================
#include "ChromaTapeProcessor.h"

// Constructor remains unchanged
ChromaTapeProcessor::ChromaTapeProcessor(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    mainApvts(apvts)
{
    // Parameter ID assignments (Unchanged)
    auto slotPrefix = "SLOT_" + juce::String(slotIndex + 1) + "_CT_";
    saturationParamIds[0] = slotPrefix + "LOW_SATURATION";
    saturationParamIds[1] = slotPrefix + "MID_SATURATION";
    saturationParamIds[2] = slotPrefix + "HIGH_SATURATION";

    wowParamIds[0] = slotPrefix + "LOW_WOW";
    wowParamIds[1] = slotPrefix + "MID_WOW";
    wowParamIds[2] = slotPrefix + "HIGH_WOW";

    flutterParamIds[0] = slotPrefix + "LOW_FLUTTER";
    flutterParamIds[1] = slotPrefix + "MID_FLUTTER";
    flutterParamIds[2] = slotPrefix + "HIGH_FLUTTER";

    lowMidCrossoverParamId = slotPrefix + "LOWMID_CROSS";
    midHighCrossoverParamId = slotPrefix + "MIDHIGH_CROSS";

    // NEW: Assign IDs for new parameters
    scrapeParamId = slotPrefix + "SCRAPE_FLUTTER";
    chaosParamId = slotPrefix + "CHAOS_AMOUNT";
    hissParamId = slotPrefix + "HISS_LEVEL";
    humParamId = slotPrefix + "HUM_LEVEL";
    headBumpFreqParamId = slotPrefix + "HEADBUMP_FREQ";
    headBumpGainParamId = slotPrefix + "HEADBUMP_GAIN";
}

ChromaTapeProcessor::~ChromaTapeProcessor()
{
}

// Updated prepareToPlay
void ChromaTapeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels() };
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // 1. Prepare Bands
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        auto& band = bands[i];
        band.saturator.prepare(spec);

        // NEW: Initialize Hysteresis State (Blueprint 2.3)
        band.hysteresis_last_input.resize(spec.numChannels, 0.0f);

        // NEW: Initialize EQ Stages (Blueprint 2.2, 2.4)
        band.headBumpFilters.resize(spec.numChannels);
        for (auto& filter : band.headBumpFilters) {
            filter.prepare(monoSpec);
        }

        band.dynamicHfFilter.prepare(spec);
        band.hfEnvelope.prepare(spec);

        if (i == HIGH)
        {
            band.dynamicHfFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            band.hfEnvelope.setAttackTime(5.0f);
            band.hfEnvelope.setReleaseTime(50.0f);
        }

        // --- Modulation Initialization ---
        int maxDelaySamples = (int)(sampleRate * 0.030) + 2; // 30ms max delay
        band.delayLine.prepare(spec);
        band.delayLine.setMaximumDelayInSamples(maxDelaySamples);

        band.wowLFO.prepare(spec);
        band.flutterLFO.prepare(spec);
        band.wowLFO.setFrequency(1.0f);
        band.flutterLFO.setFrequency(15.0f);

        band.wowLFO.setStereoOffset(0.2f);
        band.flutterLFO.setStereoOffset(0.15f);

        // Noise/Smoothing setup
        band.noiseGen.setType(DSPUtils::NoiseGenerator::NoiseType::White);
        band.noiseFilter.prepare(monoSpec); // Use monoSpec (control signal)

        // OPTIMIZATION FIX: Initialize Stereo Mod Smoothers
        band.modSmoothers.resize(spec.numChannels);
        for (auto& smoother : band.modSmoothers)
        {
            smoother.prepare(monoSpec); // Prepare individual instances with monoSpec
            smoother.setCutoffFrequency(100.0f); // Example smoothing frequency
        }

        // Scrape Flutter Setup
        band.scrapeNoiseFilter.prepare(monoSpec); // Use monoSpec (control signal)
        band.scrapeNoiseFilter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        band.scrapeNoiseFilter.setCutoffFrequency(3000.0f);
        band.scrapeNoiseFilter.setResonance(0.5f);

        // Initialize Parameter Smoothers
        band.smoothedWowDepth.reset(sampleRate, 0.05);
        band.smoothedFlutterDepth.reset(sampleRate, 0.05);
        band.smoothedSaturationDb.reset(sampleRate, 0.05);
    }

    // 2. Prepare Crossover Network
    crossover.prepare(spec);

    // 3. Prepare Noise/Hum
    hissGenerator.setType(DSPUtils::NoiseGenerator::NoiseType::Pink);
    hissShapingFilters.resize(spec.numChannels);
    auto hissCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 4000.0f, 0.5f, 6.0f);
    for (auto& filter : hissShapingFilters) {
        filter.prepare(monoSpec);
        *filter.coefficients = *hissCoeffs;
    }

    humOscillator.prepare(spec);
    humOscillator.setFrequency(60.0f);
    humHarmonicOscillator.prepare(spec);
    humHarmonicOscillator.setFrequency(180.0f);

    // 4. OPTIMIZATION FIX: Initialize Global Parameter Smoothers
    double smoothingTime = 0.05; // 50ms smoothing
    smoothedScrape.reset(sampleRate, smoothingTime);
    smoothedChaos.reset(sampleRate, smoothingTime);
    smoothedHissLevel.reset(sampleRate, smoothingTime);
    smoothedHumLevel.reset(sampleRate, smoothingTime);

    reset();
}

void ChromaTapeProcessor::releaseResources()
{
    reset();
}

// Updated reset
void ChromaTapeProcessor::reset()
{
    for (auto& band : bands)
    {
        band.saturator.reset();
        band.delayLine.reset();
        band.wowLFO.reset();
        band.flutterLFO.reset();
        band.noiseFilter.reset();

        // OPTIMIZATION FIX: Reset Stereo Mod Smoothers
        for (auto& smoother : band.modSmoothers)
        {
            smoother.reset();
        }

        for (auto& filter : band.headBumpFilters) filter.reset();
        band.dynamicHfFilter.reset();
        band.hfEnvelope.reset();
        std::fill(band.hysteresis_last_input.begin(), band.hysteresis_last_input.end(), 0.0f);
        band.scrapeNoiseFilter.reset();
        band.chaos_state = 0.5f;

        band.smoothedWowDepth.setCurrentAndTargetValue(band.smoothedWowDepth.getTargetValue());
        band.smoothedFlutterDepth.setCurrentAndTargetValue(band.smoothedFlutterDepth.getTargetValue());
        band.smoothedSaturationDb.setCurrentAndTargetValue(band.smoothedSaturationDb.getTargetValue());
    }

    crossover.reset();
    for (auto& filter : hissShapingFilters) filter.reset();
    humOscillator.reset();
    humHarmonicOscillator.reset();

    // OPTIMIZATION FIX: Reset Global Smoothers
    smoothedScrape.setCurrentAndTargetValue(smoothedScrape.getTargetValue());
    smoothedChaos.setCurrentAndTargetValue(smoothedChaos.getTargetValue());
    smoothedHissLevel.setCurrentAndTargetValue(smoothedHissLevel.getTargetValue());
    smoothedHumLevel.setCurrentAndTargetValue(smoothedHumLevel.getTargetValue());
}

// Constants
const float MAX_GAIN_LINEAR = 4.0f;
const float LOW_ASYMMETRY = 0.7f;
const float MID_ASYMMETRY_OFFSET = 0.1f;
const float HIGH_ASYMMETRY = 0.1f;

static float calculateInternalDrive(float saturationDb)
{
    if (saturationDb <= 0.01f) return 0.0f;
    float driveLinear = juce::Decibels::decibelsToGain(saturationDb);
    float internalDrive = (driveLinear - 1.0f) / (MAX_GAIN_LINEAR - 1.0f);
    return juce::jlimit(0.0f, 1.0f, internalDrive);
}

// Updated updateParameters
void ChromaTapeProcessor::updateParameters()
{
    // Added safety checks for parameter existence
    if (!mainApvts.getRawParameterValue(lowMidCrossoverParamId) || !mainApvts.getRawParameterValue(midHighCrossoverParamId)) return;

    float lowMidCross = mainApvts.getRawParameterValue(lowMidCrossoverParamId)->load();
    float midHighCross = mainApvts.getRawParameterValue(midHighCrossoverParamId)->load();
    crossover.setCrossoverFrequencies(lowMidCross, midHighCross);

    for (int i = 0; i < NUM_BANDS; ++i)
    {
        // Added safety checks
        if (mainApvts.getRawParameterValue(saturationParamIds[i]))
            bands[i].smoothedSaturationDb.setTargetValue(mainApvts.getRawParameterValue(saturationParamIds[i])->load());
        if (mainApvts.getRawParameterValue(wowParamIds[i]))
            bands[i].smoothedWowDepth.setTargetValue(mainApvts.getRawParameterValue(wowParamIds[i])->load());
        if (mainApvts.getRawParameterValue(flutterParamIds[i]))
            bands[i].smoothedFlutterDepth.setTargetValue(mainApvts.getRawParameterValue(flutterParamIds[i])->load());
    }

    // OPTIMIZATION FIX: Update Global Smoothers (Read APVTS once per block)
    if (mainApvts.getRawParameterValue(scrapeParamId))
        smoothedScrape.setTargetValue(mainApvts.getRawParameterValue(scrapeParamId)->load());
    if (mainApvts.getRawParameterValue(chaosParamId))
        smoothedChaos.setTargetValue(mainApvts.getRawParameterValue(chaosParamId)->load());

    if (mainApvts.getRawParameterValue(hissParamId)) {
        float hissDb = mainApvts.getRawParameterValue(hissParamId)->load();
        smoothedHissLevel.setTargetValue(juce::Decibels::decibelsToGain(hissDb));
    }
    if (mainApvts.getRawParameterValue(humParamId)) {
        float humDb = mainApvts.getRawParameterValue(humParamId)->load();
        smoothedHumLevel.setTargetValue(juce::Decibels::decibelsToGain(humDb));
    }

    // Check sample rate validity before calculating coefficients
    if (!bands[LOW].headBumpFilters.empty() && getSampleRate() > 0 && mainApvts.getRawParameterValue(headBumpFreqParamId) && mainApvts.getRawParameterValue(headBumpGainParamId))
    {
        float headBumpFreq = mainApvts.getRawParameterValue(headBumpFreqParamId)->load();
        float headBumpGain = mainApvts.getRawParameterValue(headBumpGainParamId)->load();
        auto headBumpCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), headBumpFreq, 0.7f, juce::Decibels::decibelsToGain(headBumpGain));
        for (auto& filter : bands[LOW].headBumpFilters) {
            *filter.coefficients = *headBumpCoeffs;
        }
    }
}

// Updated processBlock
void ChromaTapeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // 1. Update Parameters
    updateParameters();

    // 2. Split into bands
    crossover.processBlock(buffer);

    // 3. Process each band
    std::array<juce::AudioBuffer<float>*, NUM_BANDS> bandBuffers = {
        &crossover.getLowBand(), &crossover.getMidBand(), &crossover.getHighBand()
    };

    // Main Processing Loop
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // OPTIMIZATION FIX: Advance global smoothers once per sample frame
        smoothedScrape.getNextValue();
        smoothedChaos.getNextValue();

        for (int i = 0; i < NUM_BANDS; ++i)
        {
            bands[i].smoothedSaturationDb.getNextValue();
            bands[i].smoothedWowDepth.getNextValue();
            bands[i].smoothedFlutterDepth.getNextValue();

            updateModulation(i);

            processBand(i, sample, numChannels, *bandBuffers[i]);
        }
    }

    // 4. Sum bands back into output buffer
    buffer.clear();
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        auto* bandBuffer = bandBuffers[i];
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch < bandBuffer->getNumChannels())
            {
                buffer.addFrom(ch, 0, *bandBuffer, ch, 0, numSamples);
            }
        }
    }

    // 5. Inject Hiss (Blueprint IV.1)
    if (smoothedHissLevel.getTargetValue() > 1e-6f && hissShapingFilters.size() == (size_t)numChannels)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentHissLevel = smoothedHissLevel.getNextValue();
            float noise = hissGenerator.getNextSample();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                // CONFIRMED: IIR::Filter prepared with monoSpec uses 1 argument.
                float hissSample = hissShapingFilters[ch].processSample(noise) * currentHissLevel;
                buffer.addSample(ch, sample, hissSample);
            }
        }
    }
    else if (smoothedHissLevel.isSmoothing())
    {
        smoothedHissLevel.skip(numSamples);
    }
}

// The Core Processing Logic (Optimized Structure)
void ChromaTapeProcessor::processBand(int bandIdx, int sample, int numChannels, juce::AudioBuffer<float>& buffer)
{
    auto& band = bands[bandIdx];
    float saturationDb = band.smoothedSaturationDb.getCurrentValue();
    float internalDrive = calculateInternalDrive(saturationDb);

    float hum = 0.0f;
    if (bandIdx == LOW)
    {
        float currentHumLevel = smoothedHumLevel.getNextValue();
        hum = (humOscillator.getNextBipolar() + humHarmonicOscillator.getNextBipolar() * 0.5f) * currentHumLevel;
    }

    constexpr int MAX_CHANNELS = 2;
    std::array<float, MAX_CHANNELS> sat_outputs;
    int activeChannels = juce::jmin(numChannels, MAX_CHANNELS);

    // === Stages 1 & 2: EQ and Saturation (Per Channel) ===
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        float input = buffer.getSample(ch, sample);

        if (bandIdx == LOW)
        {
            input += hum;
            if (ch < (int)band.headBumpFilters.size())
            {
                // CONFIRMED: IIR::Filter prepared with monoSpec uses 1 argument.
                input = band.headBumpFilters[ch].processSample(input);
            }
        }

        float sat_out = input;
        if (internalDrive > 0.0f)
        {
            band.saturator.setDrive(internalDrive);

            if (bandIdx == LOW)
            {
                band.saturator.setAsymmetry(LOW_ASYMMETRY);
            }
            else if (bandIdx == MID)
            {
                if (ch < (int)band.hysteresis_last_input.size())
                {
                    if (input > band.hysteresis_last_input[ch])
                        band.saturator.setAsymmetry(MID_ASYMMETRY_OFFSET);
                    else
                        band.saturator.setAsymmetry(-MID_ASYMMETRY_OFFSET);

                    band.hysteresis_last_input[ch] = input;
                }
            }
            else
            {
                band.saturator.setAsymmetry(HIGH_ASYMMETRY);
            }

            sat_out = band.saturator.processSample(ch, input);

            if (saturationDb > 1e-6f) {
                sat_out *= (1.0f / juce::Decibels::decibelsToGain(saturationDb));
            }
        }

        sat_outputs[ch] = sat_out;
    }

    // === Stage 2.5: Calculate Envelope (OPTIMIZATION: Once per frame) ===
    if (bandIdx == HIGH)
    {
        float maxAbsSat = 0.0f;
        for (int ch = 0; ch < activeChannels; ++ch)
        {
            maxAbsSat = juce::jmax(maxAbsSat, std::abs(sat_outputs[ch]));
        }

        float envelope = band.hfEnvelope.process(maxAbsSat);
        float cutoffHz = juce::jmap(juce::jlimit(0.0f, 0.5f, envelope), 0.0f, 0.5f, 20000.0f, 6000.0f);
        band.dynamicHfFilter.setCutoffFrequency(cutoffHz);
    }

    // === Stages 3 & 4: Post-Filtering and Modulation (Per Channel) ===
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        float processed_sample = sat_outputs[ch];

        if (bandIdx == HIGH)
        {
            processed_sample = band.dynamicHfFilter.processSample(ch, processed_sample);
        }

        float mod_out = applyModulation(bandIdx, ch, processed_sample);
        buffer.setSample(ch, sample, mod_out);
    }
}

// Blueprint III - Update LFOs, Chaos, and Noise (Once per frame per band)
void ChromaTapeProcessor::updateModulation(int bandIdx)
{
    auto& band = bands[bandIdx];

    band.chaos_state = band.CHAOS_R * band.chaos_state * (1.0f - band.chaos_state);

    float chaosAmount = smoothedChaos.getCurrentValue();

    if (chaosAmount > 0.001f)
    {
        float chaos_bipolar = (band.chaos_state * 2.0f - 1.0f) * chaosAmount;
        float wowFreq = 1.0f * (1.0f + chaos_bipolar * 0.2f);
        float flutterFreq = 15.0f * (1.0f + chaos_bipolar * 0.2f);
        band.wowLFO.setFrequency(wowFreq);
        band.flutterLFO.setFrequency(flutterFreq);
    }

    band.currentWow = band.wowLFO.getNextStereoSample();
    band.currentFlutter = band.flutterLFO.getNextStereoSample();

    float noise = band.noiseGen.getNextSample();

    // FIX: TPT and SVT filters require the channel index (0 for mono).
    band.currentFilteredNoise = band.noiseFilter.processSample(0, noise);
    band.currentScrapeNoise = band.scrapeNoiseFilter.processSample(0, noise);
}

// Blueprint III - Calculate and Apply Delay (Optimized)
float ChromaTapeProcessor::applyModulation(int bandIdx, int channel, float inputSample)
{
    auto& band = bands[bandIdx];
    float sr = (float)getSampleRate();

    const float MAX_WOW_MS = 10.0f;
    const float MAX_FLUTTER_MS = 2.0f;
    const float MAX_SCRAPE_MS = 0.5f;
    const float BASE_DELAY_MS = 15.0f;

    float wowDepth = band.smoothedWowDepth.getCurrentValue();
    float flutterDepth = band.smoothedFlutterDepth.getCurrentValue();
    float scrapeDepth = smoothedScrape.getCurrentValue();

    float wowMod = (channel == 0) ? band.currentWow.first : band.currentWow.second;
    float periodicFlutter = (channel == 0) ? band.currentFlutter.first : band.currentFlutter.second;

    float wowModMs = wowMod * wowDepth * MAX_WOW_MS * 0.5f;
    float flutterModMs = (periodicFlutter * 0.7f + band.currentFilteredNoise * 0.3f) * flutterDepth * MAX_FLUTTER_MS * 0.5f;
    float scrapeModMs = band.currentScrapeNoise * scrapeDepth * MAX_SCRAPE_MS;

    float rawModulationMs = BASE_DELAY_MS + wowModMs + flutterModMs + scrapeModMs;

    float smoothedDelayMs = rawModulationMs;
    if (channel < (int)band.modSmoothers.size())
    {
        // FIX: FirstOrderTPTFilter requires the channel index (0 for mono).
        smoothedDelayMs = band.modSmoothers[channel].processSample(0, rawModulationMs);
    }

    float delaySamples = smoothedDelayMs * sr / 1000.0f;
    delaySamples = juce::jmax(0.1f, juce::jmin(delaySamples, (float)band.delayLine.getMaximumDelayInSamples() - 1.0f));

    band.delayLine.pushSample(channel, inputSample);
    return band.delayLine.popSample(channel, delaySamples, true);
}


// Crossover Network Implementation
void ChromaTapeProcessor::CrossoverNetwork::prepare(const juce::dsp::ProcessSpec& spec)
{
    lowMidLowpass.prepare(spec);
    lowMidHighpass.prepare(spec);
    midHighLowpass.prepare(spec);
    midHighHighpass.prepare(spec);

    lowMidLowpass.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowMidHighpass.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    midHighLowpass.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    midHighHighpass.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    int numChannels = (int)spec.numChannels;
    int maxSamples = (int)spec.maximumBlockSize;

    lowBand.setSize(numChannels, maxSamples);
    midBand.setSize(numChannels, maxSamples);
    highBand.setSize(numChannels, maxSamples);
}

void ChromaTapeProcessor::CrossoverNetwork::reset()
{
    lowMidLowpass.reset();
    lowMidHighpass.reset();
    midHighLowpass.reset();
    midHighHighpass.reset();
}

void ChromaTapeProcessor::CrossoverNetwork::setCrossoverFrequencies(float lowMid, float midHigh)
{
    midHigh = juce::jmax(lowMid + 20.0f, midHigh);

    lowMidLowpass.setCutoffFrequency(lowMid);
    lowMidHighpass.setCutoffFrequency(lowMid);
    midHighLowpass.setCutoffFrequency(midHigh);
    midHighHighpass.setCutoffFrequency(midHigh);
}

void ChromaTapeProcessor::CrossoverNetwork::processBlock(juce::AudioBuffer<float>& buffer)
{
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // Ensure buffers are correctly sized
    lowBand.setSize(numChannels, numSamples, false, false, true);
    midBand.setSize(numChannels, numSamples, false, false, true);
    highBand.setSize(numChannels, numSamples, false, false, true);

    // Process Low Band
    lowBand.makeCopyOf(buffer);
    juce::dsp::AudioBlock<float> lowBlock(lowBand);
    juce::dsp::ProcessContextReplacing<float> lowContext(lowBlock);
    lowMidLowpass.process(lowContext);

    // Process High Band (initially contains input signal)
    highBand.makeCopyOf(buffer);
    juce::dsp::AudioBlock<float> highBlock(highBand);
    juce::dsp::ProcessContextReplacing<float> highContext(highBlock);
    lowMidHighpass.process(highContext); // highBand now contains frequencies > lowMid

    // Process Mid Band (copy from the partially filtered highBand)
    midBand.makeCopyOf(highBand);
    juce::dsp::AudioBlock<float> midBlock(midBand);
    juce::dsp::ProcessContextReplacing<float> midContext(midBlock);
    midHighLowpass.process(midContext); // midBand now contains frequencies between lowMid and midHigh

    // Finalize High Band
    midHighHighpass.process(highContext); // highBand now contains frequencies > midHigh
}