//================================================================================
// File: FX_Modules/SpectralDiffuser.cpp
//================================================================================
#include "SpectralDiffuser.h"

// JUCE 8 FIX (C2084): Remove the constructor implementation here.

void SpectralDiffuser::prepare(const juce::dsp::ProcessSpec& spec)
{
    int numChannels = (int)spec.numChannels;

    // Initialize FIFOs (size FFT_SIZE)
    inputFIFO.setSize(numChannels, FFT_SIZE);
    outputFIFO.setSize(numChannels, FFT_SIZE);

    // Initialize FFT data buffers (size 2*FFT_SIZE)
    fftData.resize(numChannels);
    for (auto& channelData : fftData)
    {
        channelData.resize(FFT_SIZE * 2);
    }

    reset();
}

void SpectralDiffuser::reset()
{
    fifoIndex = 0;
    inputFIFO.clear();
    outputFIFO.clear();
}

// FIX: Corrected the loop structure (Samples outer, Channels inner) for proper OLA implementation.
// The original implementation iterated Channel first, then Sample, breaking time alignment.
void SpectralDiffuser::process(juce::AudioBuffer<float>& buffer, float diffusionAmount)
{
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // Bypass processing if diffusion is negligible
    if (diffusionAmount < 0.001f)
        return;

    // Iterate over samples first (Outer loop)
    for (int i = 0; i < numSamples; ++i)
    {
        // Iterate over channels for the current sample index 'i' (Inner loop)
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float inputSample = buffer.getSample(channel, i);

            // 1. Push input sample into Input FIFO
            inputFIFO.setSample(channel, fifoIndex, inputSample);

            // 2. Retrieve output sample from Output FIFO (delayed by HOP_SIZE)
            float outputSample = outputFIFO.getSample(channel, fifoIndex);
            buffer.setSample(channel, i, outputSample);

            // Clear the sample we just read
            outputFIFO.setSample(channel, fifoIndex, 0.0f);
        }

        // 3. Advance FIFO index (Done after all channels are processed for this time step)
        fifoIndex++;

        // 4. Check if the input FIFO is full (ready for FFT)
        if (fifoIndex == FFT_SIZE)
        {
            // Process the frame for all channels
            for (int ch = 0; ch < numChannels; ++ch)
            {
                processFrame(ch, diffusionAmount);

                // Shift the input FIFO to handle the overlap
                // Move the second half (at index HOP_SIZE) to the beginning (at index 0).
                for (int j = 0; j < HOP_SIZE; ++j)
                {
                    inputFIFO.setSample(ch, j, inputFIFO.getSample(ch, j + HOP_SIZE));
                }
            }
            // Reset index to the start of the next frame accumulation
            fifoIndex = HOP_SIZE;
        }
    }
}

void SpectralDiffuser::processFrame(int channel, float diffusionAmount)
{
    auto& data = fftData[channel];

    // 1. Copy Input FIFO to FFT buffer
    std::copy(inputFIFO.getReadPointer(channel),
        inputFIFO.getReadPointer(channel) + FFT_SIZE,
        data.begin());

    // 2. Apply Analysis Window
    window.multiplyWithWindowingTable(data.data(), FFT_SIZE);

    // 3. Perform Forward FFT (in-place)
    fft.performRealOnlyForwardTransform(data.data());

    // 4. Process Spectrum (Phase Randomization)
    // FIX: Iterate from bin 1 up to (but not including) Nyquist (i < FFT_SIZE/2).
    // We MUST skip DC (i=0) and Nyquist (i=FFT_SIZE/2) as their phase must remain fixed for real signals.
    // The original code incorrectly iterated over and randomized these bins.
    for (int i = 1; i < FFT_SIZE / 2; ++i)
    {
        float real = data[2 * i];
        float imag = data[2 * i + 1];

        // Convert to polar coordinates
        float magnitude = std::sqrt(real * real + imag * imag);
        float phase = std::atan2(imag, real);

        // Add random phase shift scaled by diffusion amount
        phase += distribution(randomEngine) * diffusionAmount;

        // Convert back to rectangular coordinates
        data[2 * i] = magnitude * std::cos(phase);
        data[2 * i + 1] = magnitude * std::sin(phase);
    }
    // DC (data[0]) and Nyquist (data[1]) components remain untouched.


    // 5. Perform Inverse FFT (in-place)
    fft.performRealOnlyInverseTransform(data.data());

    // 6. Apply Synthesis Window
    window.multiplyWithWindowingTable(data.data(), FFT_SIZE);

    // 7. Overlap-Add into Output FIFO
    // We add the result starting at the beginning of the output FIFO.
    // For Hann window with 50% overlap, the gain is constant if the window normalization is correct (handled by JUCE).
    for (int i = 0; i < FFT_SIZE; ++i)
    {
        outputFIFO.addSample(channel, i, data[i]);
    }
}