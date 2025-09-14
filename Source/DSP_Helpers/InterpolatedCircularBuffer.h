//================================================================================
// File: DSP_Helpers/InterpolatedCircularBuffer.h
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

// Implements Cubic interpolation.
class InterpolatedCircularBuffer
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec, int sizeInSamples)
    {
        // Add margins for interpolation safety at boundaries
        buffer.setSize((int)spec.numChannels, sizeInSamples + INTERP_MARGIN * 2);
        bufferSize = sizeInSamples;
        numChannels = (int)spec.numChannels;
        writePos = INTERP_MARGIN; // Start writing after the initial margin
        reset();
    }

    void reset()
    {
        buffer.clear();
        writePos = INTERP_MARGIN;
        if (bufferSize > 0)
        {
            updateMargins();
        }
    }

    // Write block into the circular buffer
    void write(const juce::dsp::AudioBlock<float>& block)
    {
        int numSamples = (int)block.getNumSamples();
        int blockChannels = (int)block.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (ch < blockChannels)
                    buffer.setSample(ch, writePos, block.getSample(ch, i));
            }

            advanceWritePosition();
        }
    }

    // NEW: Writes a single sample to a specific channel without advancing the write head.
    void writeSample(int channel, float sampleValue)
    {
        if (juce::isPositiveAndBelow(channel, numChannels))
            buffer.setSample(channel, writePos, sampleValue);
    }

    // NEW: Advances the write head for all channels. Call this once per sample frame.
    void advanceWritePosition()
    {
        writePos++;
        // Handle wraparound and update margins
        if (writePos >= bufferSize + INTERP_MARGIN)
        {
            writePos = INTERP_MARGIN;
            updateMargins();
        }
    }

    // Read interpolated sample at a fractional position (relative to logical buffer bounds [0, bufferSize))
    float read(int channel, float fractionalPosition)
    {
        if (channel >= numChannels || bufferSize == 0) return 0.0f;

        // Ensure position is within logical bounds [0, bufferSize) using fmod
        fractionalPosition = std::fmod(fractionalPosition, (float)bufferSize);
        if (fractionalPosition < 0.0f) fractionalPosition += (float)bufferSize;

        // Adjust position relative to the physical start (INTERP_MARGIN)
        float physicalPosition = fractionalPosition + INTERP_MARGIN;

        const float* data = buffer.getReadPointer(channel);

        // Cubic Interpolation
        int i0 = (int)std::floor(physicalPosition);
        float fraction = physicalPosition - (float)i0;

        // Indices are safe because of the margins
        float ym1 = data[i0 - 1];
        float y0 = data[i0];
        float y1 = data[i0 + 1];
        float y2 = data[i0 + 2];

        // Optimized Cubic interpolation formula
        float c0 = y0;
        float c1 = 0.5f * (y1 - ym1);
        float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);

        return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
    }

    int getSize() const { return bufferSize; }

    // FIX: Added getNumChannels accessor required by FractureTubeProcessor
    int getNumChannels() const { return numChannels; }

    // Returns the current write position relative to the logical start (0)
    int getWritePosition() const
    {
        return writePos - INTERP_MARGIN;
    }

private:
    // Updates the margins by copying data from the opposite end of the buffer.
    void updateMargins()
    {
        // Copy end of logical buffer to beginning margin
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.copyFrom(ch, 0, buffer, ch, bufferSize, INTERP_MARGIN);
        }
        // Copy beginning of logical buffer to end margin
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.copyFrom(ch, bufferSize + INTERP_MARGIN, buffer, ch, INTERP_MARGIN, INTERP_MARGIN);
        }
    }

    juce::AudioBuffer<float> buffer;
    int bufferSize = 0;
    int writePos = 0;
    int numChannels = 0;
    static constexpr int INTERP_MARGIN = 4;
};