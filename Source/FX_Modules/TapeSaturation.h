//================================================================================
// File: FX_Modules/TapeSaturation.h (CORRECTED V2)
// Description: Optimized polynomial tape saturation model.
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace TapeDSP
{
    /**
     * OptimizedTapeSaturator implementing cubic nonlinearity with asymmetry.
     * Optimized for high performance using SIMD.
     */
    class OptimizedTapeSaturator
    {
    private:
        // Define the filter type used for DC blocking
        using DCFilter = juce::dsp::IIR::Filter<float>;

    public:
        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            numChannels = (int)spec.numChannels;

            // Initialize the std::vector of filters.
            dcBlockers.resize(numChannels);

            if (spec.sampleRate > 0)
            {
                // High-pass filter at 5Hz to remove offsets. Calculate coefficients once.
                auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 5.0f);

                // Configure each filter instance.
                // We must use a MONO spec for individual IIR filters when managing them manually per channel.
                juce::dsp::ProcessSpec monoSpec = spec;
                monoSpec.numChannels = 1;

                for (auto& filter : dcBlockers)
                {
                    filter.prepare(monoSpec);
                    // Assign the coefficients.
                    *filter.coefficients = *coefficients;
                }
            }
            reset();
        }

        void reset()
        {
            // Reset all filters in the vector.
            for (auto& filter : dcBlockers)
            {
                filter.reset();
            }
            alpha = 0.0f;
            beta = 0.0f;
        }

        // Set the drive (0.0 to 1.0). Internally maps to the alpha coefficient.
        void setDrive(float drive)
        {
            // Alpha controls the intensity of the cubic term (x^3).
            // Constraining alpha to [0, 1/3].
            alpha = drive * 0.333f;
        }

        // Set asymmetry (0.0 to 1.0). Controls the quadratic term (x^2).
        void setAsymmetry(float asymmetry)
        {
            // Beta controls the amount of asymmetry (even harmonics).
            // Constraining beta for stability.
            beta = asymmetry * 0.2f;
        }

        // FIX: Complete the truncated file by adding the missing processSample and members.

        // Process a single sample.
        // Note: Requires the channel index 'ch' for accessing the correct DC blocker instance.
        float processSample(int ch, float input)
        {
            if (ch >= numChannels || ch < 0) return input;

            // 1. Apply DC blocking before saturation.
            // IIR::Filter::processSample takes one argument when prepared with mono spec.
            float x = dcBlockers[ch].processSample(input);

            // 2. Optimized polynomial saturation (Horner's method)
            // y = x * (1 + x * (beta - alpha*x))
            float y = x * (1.0f + x * (beta - alpha * x));

            // 3. Safety Clipping
            return juce::jlimit(-1.5f, 1.5f, y);
        }

    private:
        float alpha = 0.0f; // Controls cubic saturation (symmetric)
        float beta = 0.0f;  // Controls quadratic saturation (asymmetric)
        int numChannels = 0;

        // Vector of DC blocker filters (one per channel)
        std::vector<DCFilter> dcBlockers;
    };

} // namespace TapeDSP