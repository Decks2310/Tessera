//================================================================================
// File: DSP_Helpers/TransientDetector.cpp
//================================================================================
#include "TransientDetector.h"

TransientDetector::TransientDetector()
    : fft(FFT_ORDER),
    // FIX: Use the correct JUCE 8 syntax with the WindowingMethod enum.
    window(FFT_SIZE, juce::dsp::WindowingFunction<float>::WindowingMethod::hann)
{
}

void TransientDetector::prepare(const juce::dsp::ProcessSpec& spec)
{
    // Initialize buffers
    inputFIFO.resize(FFT_SIZE, 0.0f);
    fftData.resize(FFT_SIZE * 2, 0.0f); // JUCE requires 2*N for real-only FFT
    int numBins = FFT_SIZE / 2 + 1;
    currentMagnitudes.resize(numBins, 0.0f);
    previousMagnitudes.resize(numBins, 0.0f);

    // Initialize smoothing (e.g., 20ms response time for smooth control signal)
    smoothedFlux.reset(spec.sampleRate, 0.02);

    reset();
}

void TransientDetector::reset()
{
    fifoIndex = 0;
    std::fill(inputFIFO.begin(), inputFIFO.end(), 0.0f);
    std::fill(previousMagnitudes.begin(), previousMagnitudes.end(), 0.0f);
    smoothedFlux.setCurrentAndTargetValue(0.0f);
}

// FIX: Removed process(block) and replaced with processSample(monoSample)
// void TransientDetector::process(const juce::dsp::AudioBlock<float>& block) { ... } // REMOVED

// NEW IMPLEMENTATION: Process sample-by-sample
void TransientDetector::processSample(float monoSample)
{
    // Safety check for buffer size
    if (fifoIndex >= (int)inputFIFO.size())
    {
        // Advance smoother anyway to maintain timing.
        smoothedFlux.getNextValue();
        return;
    }

    // Push sample into FIFO
    inputFIFO[fifoIndex] = monoSample;
    fifoIndex++;

    // Check if a frame is ready
    if (fifoIndex == FFT_SIZE)
    {
        processFrame();
        // Shift FIFO for overlap
        // Ensure inputFIFO is large enough before copying (safety check)
        if ((int)inputFIFO.size() >= FFT_SIZE)
        {
            std::copy(inputFIFO.begin() + HOP_SIZE, inputFIFO.end(), inputFIFO.begin());
        }
        fifoIndex = FFT_SIZE - HOP_SIZE; // Corrected index after shift
    }

    // CRITICAL: Advance the smoother sample-by-sample for accurate control signal generation
    smoothedFlux.getNextValue();
}

void TransientDetector::processFrame() {
    // 1. Copy FIFO to FFT buffer and apply window
    std::copy(inputFIFO.begin(), inputFIFO.end(), fftData.begin());
    window.multiplyWithWindowingTable(fftData.data(), FFT_SIZE);

    // 2. Perform Forward FFT
    fft.performRealOnlyForwardTransform(fftData.data());

    // 3. Calculate Magnitudes and Flux
    float flux = 0.0f;
    int numBins = FFT_SIZE / 2 + 1;

    for (int i = 0; i < numBins; ++i)
    {
        float real, imag;
        // Unpack JUCE format
        if (i == 0) { real = fftData[0]; imag = 0.0f; } // DC
        else if (i == FFT_SIZE / 2) { real = fftData[1]; imag = 0.0f; } // Nyquist
        else { real = fftData[2 * i]; imag = fftData[2 * i + 1]; }

        float magnitude = std::sqrt(real * real + imag * imag);
        currentMagnitudes[i] = magnitude;

        // Calculate flux (rectified difference)
        float diff = magnitude - previousMagnitudes[i];
        if (diff > 0)
            flux += diff;
    }

    // 4. Normalize and set target for smoothing
    // Normalization factor is empirical. Tuned for responsiveness.
    float normalizedFlux = juce::jlimit(0.0f, 1.0f, flux / 5.0f);
    smoothedFlux.setTargetValue(normalizedFlux);

    // 5. Update previous magnitudes
    std::copy(currentMagnitudes.begin(), currentMagnitudes.end(), previousMagnitudes.begin());
}
