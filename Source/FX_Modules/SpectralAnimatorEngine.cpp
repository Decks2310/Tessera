//================================================================================
// File: FX_Modules/SpectralAnimatorEngine.cpp
//================================================================================
#include "SpectralAnimatorEngine.h"

SpectralAnimatorEngine::SpectralAnimatorEngine()
    : forwardFFT(FFT_ORDER),
    // Initialize Hann window (suitable for 75% overlap)
    window(FFT_SIZE, juce::dsp::WindowingFunction<float>::WindowingMethod::hann)
{
    harmonicMask.resize(NUM_BINS, 0.0f);
    formantMask.resize(NUM_BINS, 0.0f);
}

// Helper function for Vowel Space Interpolation (Formant Mode)
SpectralAnimatorEngine::FormantProfile SpectralAnimatorEngine::getVowel(float x, float y)
{
    // X-axis: F2 (Front/Back) -> High X = Front ('i'), Low X = Back ('u')
    // Y-axis: F1 (Open/Close) -> High Y = Open ('a'), Low Y = Close ('i'/'u')

    // Define corner vowels (F1, F2) - typical adult male approximations
    const FormantProfile i = { 270, 2290 }; // Close Front ("see")
    const FormantProfile u = { 300, 870 };  // Close Back ("boot")
    const FormantProfile a = { 730, 1090 }; // Open Mid/Back ("father")
    const FormantProfile ae = { 660, 1720 }; // Open Front ("had")

    auto interpolate = [](float v1, float v2, float t) { return v1 * (1.0f - t) + v2 * t; };

    // Bilinear interpolation
    float f1_close = interpolate(u.f1, i.f1, x);
    float f1_open = interpolate(a.f1, ae.f1, x);
    float f1 = interpolate(f1_close, f1_open, y);

    float f2_close = interpolate(u.f2, i.f2, x);
    float f2_open = interpolate(a.f2, ae.f2, x);
    float f2 = interpolate(f2_close, f2_open, y);

    return { f1, f2 };
}


void SpectralAnimatorEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = (int)spec.numChannels;

    // 1. Initialize Buffers
    inputFIFO.setSize(numChannels, FFT_SIZE);
    // Output Buffer size: 2 * FFT_SIZE (provides ample space for OLA accumulation)
    int outputBufferSize = FFT_SIZE * 2;
    outputBuffer.setSize(numChannels, outputBufferSize);

    // Initialize temporary FFT buffers
    channelTimeDomain.resize(numChannels);
    channelFreqDomain.resize(numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        channelTimeDomain[ch].resize(FFT_SIZE, 0.0f);
        // JUCE requires 2*N space for real-only FFT operations
        channelFreqDomain[ch].resize(FFT_SIZE * 2, 0.0f);
    }

    // 2. Initialize Transient Detectors (Per Channel)
    transientDetectors.resize(numChannels);
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;

    // 30ms decay for the mix envelope (as per design document)
    float decayTimeMs = 30.0f;

    for (auto& detector : transientDetectors)
    {
        detector.highPassFilter.prepare(monoSpec);
        // Set high-pass cutoff (e.g., 2kHz)
        detector.highPassFilter.setCutoffFrequency(2000.0f);

        detector.envelopeFollower.prepare(monoSpec);
        // Fast attack/release for the detection signal itself
        detector.envelopeFollower.setAttackTime(1.0f);
        detector.envelopeFollower.setReleaseTime(10.0f);

        // Calculate time-based exponential decay factor for the mix control
        if (sampleRate > 0)
            detector.decayFactor = std::exp(-1.0f / (float)(sampleRate * decayTimeMs / 1000.0f));
    }

    // FIX: Initialize smoothers (e.g., 5ms smoothing time for fast response)
    double smoothingTime = 0.005;
    smoothedMorph.reset(sampleRate, smoothingTime);
    smoothedTransientPreservation.reset(sampleRate, smoothingTime);

    reset();
}

void SpectralAnimatorEngine::reset()
{
    inputFIFO.clear();
    outputBuffer.clear();
    fifoIndex = 0;
    outputBufferWritePos = 0;
    outputBufferReadPos = 0;

    for (auto& detector : transientDetectors)
    {
        detector.highPassFilter.reset();
        detector.envelopeFollower.reset();
        detector.transientMix = 0.0f;
    }

    // FIX: Reset smoothers to default values (1.0)
    smoothedMorph.setCurrentAndTargetValue(1.0f);
    smoothedTransientPreservation.setCurrentAndTargetValue(1.0f);

    masksNeedUpdate = true;
}

// Parameter setters (trigger mask updates if necessary)
void SpectralAnimatorEngine::setMode(Mode newMode) { if (currentMode != newMode) { currentMode = newMode; masksNeedUpdate = true; } }
void SpectralAnimatorEngine::setPitch(float newPitchHz) { if (pitchHz != newPitchHz) { pitchHz = newPitchHz; masksNeedUpdate = true; } }
void SpectralAnimatorEngine::setFormant(float x, float y) { formantXY = { x, y }; masksNeedUpdate = true; }
void SpectralAnimatorEngine::setMorph(float amount) { smoothedMorph.setTargetValue(amount); }
void SpectralAnimatorEngine::setTransientPreservation(float amount) { smoothedTransientPreservation.setTargetValue(amount); }


// Main process loop implementing STFT buffering and transient mixing
void SpectralAnimatorEngine::process(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int outputBufferSize = outputBuffer.getNumSamples();

    // Update masks if parameters changed
    if (masksNeedUpdate)
    {
        updateMasks();
        masksNeedUpdate = false;
    }

    // Main audio loop
    for (int i = 0; i < numSamples; ++i)
    {
        bool frameReady = false;

        // FIX (C4189): Advance smoothers once per sample frame.
        // We must advance the smoother, but we don't use the 'morph' value in this loop scope.
        smoothedMorph.getNextValue();
        float currentTransientPreservation = smoothedTransientPreservation.getNextValue();


        for (int ch = 0; ch < numChannels; ++ch)
        {
            float inputSample = buffer.getSample(ch, i);

            // --- Transient Detection Path (2.2 Logic) ---
            auto& detector = transientDetectors[ch];

            // Note: Filters/Followers prepared with monoSpec require channel index 0 when calling processSample
            float highPassed = detector.highPassFilter.processSample(0, inputSample);
            float envelope = detector.envelopeFollower.processSample(0, std::abs(highPassed));

            // Fast attack, exponential decay mix control (The "transient preservation envelope")
            if (envelope > transientThreshold)
                detector.transientMix = 1.0f; // Attack immediately
            else
                detector.transientMix *= detector.decayFactor; // Decay smoothly


            // --- STFT Path (2.2 Logic) ---

            // 1. Push input sample into FIFO
            inputFIFO.setSample(ch, fifoIndex, inputSample);

            // 3. Retrieve output sample from OLA buffer
            float outputSample = outputBuffer.getSample(ch, outputBufferReadPos);
            // Clear the sample we just read for the next accumulation
            outputBuffer.setSample(ch, outputBufferReadPos, 0.0f);

            // 4. Final Mix (Transient Integration)
            // FIX: Use the smoothed value (currentTransientPreservation)
            float mixControl = detector.transientMix * currentTransientPreservation;
            // Linear crossfade: Wet * (1-Mix) + Dry * Mix
            float finalSample = outputSample * (1.0f - mixControl) + inputSample * mixControl;

            buffer.setSample(ch, i, finalSample);
        }

        // Advance indices
        fifoIndex++;
        outputBufferReadPos = (outputBufferReadPos + 1) % outputBufferSize;

        // 2. Check if we have enough data for a frame (FFT_SIZE)
        if (fifoIndex >= FFT_SIZE)
        {
            frameReady = true;
            // Reset index relative to the overlap for the next accumulation phase
            fifoIndex -= HOP_SIZE;
        }

        // Process the frame if ready
        if (frameReady)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Copy data from FIFO to processing buffer (channelTimeDomain)
                // The data corresponding to the current frame is at the start of the FIFO before shifting.
                std::copy(inputFIFO.getReadPointer(ch), inputFIFO.getReadPointer(ch) + FFT_SIZE, channelTimeDomain[ch].begin());

                processFrame(ch);
            }

            // Shift input FIFO by hopSize for all channels (Efficient FIFO management)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Using std::rotate on the underlying float array
                float* data = inputFIFO.getWritePointer(ch);
                // This shifts the buffer content left by HOP_SIZE
                std::rotate(data, data + HOP_SIZE, data + FFT_SIZE);
            }

            // Advance output buffer write position for the next OLA
            outputBufferWritePos = (outputBufferWritePos + HOP_SIZE) % outputBufferSize;
        }
    }
}

// STFT core: FFT -> Modification -> IFFT -> OLA
void SpectralAnimatorEngine::processFrame(int channel)
{
    auto& timeDomain = channelTimeDomain[channel];
    auto& freqDomain = channelFreqDomain[channel];
    int outputBufferSize = outputBuffer.getNumSamples();

    // 1. Windowing (Analysis window)
    window.multiplyWithWindowingTable(timeDomain.data(), FFT_SIZE);

    // 2. Forward FFT
    // Copy time domain data to frequency domain buffer for FFT operation
    std::copy(timeDomain.begin(), timeDomain.end(), freqDomain.begin());
    forwardFFT.performRealOnlyForwardTransform(freqDomain.data());

    // 3. Spectral Modification
    const std::vector<float>& mask = (currentMode == Mode::Pitch) ? harmonicMask : formantMask;

    // FIX: Get the current morph value for this frame
    float currentMorph = smoothedMorph.getCurrentValue();


    // Iterate over bins (including DC and Nyquist)
    for (int k = 0; k < NUM_BINS; ++k)
    {
        float real, imag;
        // Unpack JUCE packed format (DC at [0], Nyquist at [1])
        if (k == 0) { real = freqDomain[0]; imag = 0.0f; } // DC
        else if (k == NUM_BINS - 1) { real = freqDomain[1]; imag = 0.0f; } // Nyquist
        else { real = freqDomain[2 * k]; imag = freqDomain[2 * k + 1]; }

        // Calculate Magnitude and Phase (Phase Vocoder core)
        float magnitude = std::sqrt(real * real + imag * imag);
        float phase = std::atan2(imag, real);

        // Apply Shaping Mask
        float modifiedMag = magnitude * mask[k];

        // Apply Morph Control (Linear interpolation)
        // FIX: Use the current smoothed value (currentMorph)
        float finalMag = magnitude * (1.0f - currentMorph) + modifiedMag * currentMorph;

        // 4. Convert back to Complex (using original phase)
        real = finalMag * std::cos(phase);
        imag = finalMag * std::sin(phase);

        // Pack back into JUCE format
        if (k == 0) { freqDomain[0] = real; }
        else if (k == NUM_BINS - 1) { freqDomain[1] = real; }
        else { freqDomain[2 * k] = real; freqDomain[2 * k + 1] = imag; }
    }

    // 5. Perform inverse FFT (In-place on freqDomain)
    // Note: JUCE FFT handles normalization internally.
    forwardFFT.performRealOnlyInverseTransform(freqDomain.data());

    // 6. Window (Synthesis window) and overlap-add
    // Copy result back to time domain buffer for synthesis windowing
    std::copy(freqDomain.begin(), freqDomain.begin() + FFT_SIZE, timeDomain.begin());
    window.multiplyWithWindowingTable(timeDomain.data(), FFT_SIZE);

    // Overlap-Add into the output buffer starting at the current write position
    for (int i = 0; i < FFT_SIZE; ++i)
    {
        int index = (outputBufferWritePos + i) % outputBufferSize;
        // Add the windowed frame to the accumulation buffer
        outputBuffer.addSample(channel, index, timeDomain[i]);
    }
}

// Mask generation for Pitch and Formant modes
void SpectralAnimatorEngine::updateMasks()
{
    if (sampleRate <= 0) return;
    float binWidth = (float)sampleRate / (float)FFT_SIZE;

    if (currentMode == Mode::Pitch)
    {
        // Pitch Mode: Create a harmonic mask using Gaussian peaks.
        std::fill(harmonicMask.begin(), harmonicMask.end(), 0.0f);
        float f0 = pitchHz;
        if (f0 < binWidth) return;

        // Define the width (sigma) of the harmonic peaks in bins
        const float harmonicWidth = 1.5f;
        const float widthSquared = harmonicWidth * harmonicWidth;

        for (int h = 1; ; ++h)
        {
            float freq = f0 * (float)h;
            if (freq >= sampleRate / 2.0f) break;

            float binIndex = freq / binWidth;
            int centerBin = (int)(binIndex + 0.5f);

            if (centerBin >= NUM_BINS) break;

            // Create the Gaussian peak
            // Iterate over a practical range around the center bin (e.g., +/- 3*sigma)
            int range = (int)(harmonicWidth * 3.0f);
            int startBin = juce::jmax(0, centerBin - range);
            int endBin = juce::jmin(NUM_BINS - 1, centerBin + range);

            for (int i = startBin; i <= endBin; ++i)
            {
                float distance = (float)i - binIndex;
                // Gaussian function: exp(-0.5 * (x/sigma)^2)
                float gain = std::exp(-0.5f * (distance * distance) / widthSquared);
                // Ensure we take the maximum if harmonics overlap
                harmonicMask[i] = juce::jmax(harmonicMask[i], gain);
            }
        }
    }
    else if (currentMode == Mode::Formant)
    {
        // Formant Mode: Use the interpolated vowel space
        FormantProfile vowel = getVowel(formantXY.x, formantXY.y);

        std::fill(formantMask.begin(), formantMask.end(), 0.0f);

        // Define formants (F1, F2, F3) and their bandwidths
        const std::array<float, 3> freqs = { vowel.f1, vowel.f2, 2500.0f }; // F3 fixed approximation
        const std::array<float, 3> bandwidths = { 100.0f, 150.0f, 200.0f }; // In Hz

        for (int f = 0; f < 3; ++f)
        {
            float centerFreq = freqs[f];
            float bw = bandwidths[f];

            // Create a resonant peak shape (using a Lorentzian/Cauchy model approximation)
            // Gain = 1 / (1 + ((freq - center) / bandwidth)^2)
            for (int k = 0; k < NUM_BINS; ++k)
            {
                float freq = k * binWidth;
                float normalizedDistance = (freq - centerFreq) / bw;
                float gain = 1.0f / (1.0f + normalizedDistance * normalizedDistance);
                // Take the maximum if formants overlap
                formantMask[k] = juce::jmax(formantMask[k], gain);
            }
        }

        // Normalize the mask so the maximum gain is 1.0 (preserves overall energy)
        float maxGain = *std::max_element(formantMask.begin(), formantMask.end());
        if (maxGain > 0.0f)
        {
            for (float& val : formantMask) val /= maxGain;
        }
    }
}