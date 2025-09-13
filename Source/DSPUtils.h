//================================================================================
// File: DSPUtils.h
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <random>
#include <array>
#include <cmath> // Included for std::floor, std::abs

namespace DSPUtils
{
    //==============================================================================
    // NEW: Fast Math Approximations
    //==============================================================================

    // Fast approximation of tanh(x) using a Pade approximation variation.
    // Optimized for speed and sufficient accuracy for audio saturation.
    inline float fastTanh(float x)
    {
        // Clamping input range for stability of the approximation
        // The approximation is very accurate within this range.
        if (x > 4.97f) return 1.0f;
        if (x < -4.97f) return -1.0f;

        // Pade (3,3) approximation variation: x * (27 + x^2) / (27 + 9*x^2)
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // FIX: Replaced the mathematically incorrect fastSinCycle implementation.
    // The previous version outputted an asymmetric waveform from -1.0 to 3.0, causing
    // significant DC offset and feedback issues in modules like BBDCloud.

    // NEW IMPLEMENTATION (FIXED - Accurate Pade-improved Parabolic Approximation):
    // Fast approximation of sin(2*pi*x). Input x is phase from 0.0 to 1.0.
    inline double fastSinCycle(double x)
    {
        // Ensure x is in [0, 1]
        x = x - std::floor(x);

        // Map [0, 1] to [-PI, PI] for the approximation input
        const double PI = juce::MathConstants<double>::pi;
        const double TWO_PI = juce::MathConstants<double>::twoPi;

        // t is the angle in radians from -PI to PI
        double t = x * TWO_PI - PI;

        // Optimized Parabolic approximation for range [-PI, PI]
        const double B = 4.0 / PI;
        const double C = -4.0 / (PI * PI);

        // Basic parabola
        double y = B * t + C * t * std::abs(t);

        // Pade improvement step (P=0.225) improves accuracy near the peaks.
        const double P = 0.225;
        y = P * (y * std::abs(y) - y) + y;

        return y;
    }


    //==============================================================================
    // NoiseGenerator (Based on Blueprint 1.1, Doc 2 1.3)
    //==============================================================================
    class NoiseGenerator
    {
    public:
        enum class NoiseType { White, Pink };

        NoiseGenerator() : distribution(-1.0f, 1.0f)
        {
            randomEngine.seed(static_cast<unsigned long>(juce::Time::currentTimeMillis()));
            std::fill(pinkState.begin(), pinkState.end(), 0.0f);
        }

        void setType(NoiseType newType) { type = newType; }

        float getNextSample()
        {
            if (type == NoiseType::White)
            {
                return distribution(randomEngine);
            }
            else // Pink Noise (Approximation using Voss-McCartney method)
            {
                float white = distribution(randomEngine);
                // Efficient implementation of Voss-McCartney approximation
                pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
                pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
                pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
                pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
                pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
                pinkState[5] = -0.7616f * pinkState[5] - white * 0.0168980f;
                float pink = pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3] + pinkState[4] + pinkState[5] + pinkState[6] + white * 0.5362f;
                pinkState[6] = white * 0.115926f;
                return pink * 0.11f; // Normalize output level
            }
        }

    private:
        NoiseType type = NoiseType::White;
        std::minstd_rand randomEngine;
        std::uniform_real_distribution<float> distribution;
        std::array<float, 7> pinkState;
    };

    //==============================================================================
    // Advanced LFO (Based on Blueprint 1.1, Doc 2 1.3)
    // Replaces juce::dsp::Oscillator with a custom phase accumulator (0.0 to 1.0).
    //==============================================================================
    class LFO
    {
    public:
        enum class Waveform { Sine, Triangle, Saw, Square, SampleAndHold };

        LFO() = default;

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            reset();
        }

        void reset()
        {
            phase = 0.0;
            currentSNHValue = noiseGen.getNextSample();
        }

        void setFrequency(float freqHz)
        {
            if (sampleRate > 0)
                phaseIncrement = (double)freqHz / sampleRate;
            else
                phaseIncrement = 0.0;
        }

        void setWaveform(Waveform newShape) { shape = newShape; }
        // Offset from 0.0 to 1.0
        void setStereoOffset(float offset) { stereoOffset = juce::jlimit(0.0, 1.0, (double)offset); }

        // Returns a pair of L/R samples (Bipolar: -1.0 to 1.0)
        // NOTE: This advances the LFO state. Call only once per sample frame.
        std::pair<float, float> getNextStereoSample()
        {
            // Handle S&H update at the beginning of the cycle
            if (shape == Waveform::SampleAndHold && phase < phaseIncrement)
            {
                currentSNHValue = noiseGen.getNextSample();
            }

            float leftSample = (float)generateWaveform(phase);
            double rightPhase = std::fmod(phase + stereoOffset, 1.0);
            float rightSample = (float)generateWaveform(rightPhase);

            // Advance the main phase
            phase += phaseIncrement;
            if (phase >= 1.0)
                phase -= 1.0;

            return { leftSample, rightSample };
        }

        // Utility functions for mono/backward compatibility
        float getNextBipolar() { return getNextStereoSample().first; }
        float getNextUnipolar() { return (getNextStereoSample().first + 1.0f) * 0.5f; }

    private:
        double generateWaveform(double currentPhase)
        {
            switch (shape)
            {
            case Waveform::Sine:
                return fastSinCycle(currentPhase);
            case Waveform::Triangle:
                if (currentPhase < 0.5)
                    return 4.0 * currentPhase - 1.0;
                else
                    return -4.0 * currentPhase + 3.0;
            case Waveform::Saw:
                return 2.0 * currentPhase - 1.0;
            case Waveform::Square:
                return (currentPhase < 0.5) ? 1.0 : -1.0;
            case Waveform::SampleAndHold:
                return currentSNHValue;
            default:
                return 0.0;
            }
        }

        double sampleRate = 44100.0;
        double phase = 0.0;
        double phaseIncrement = 0.0;
        double stereoOffset = 0.0;
        Waveform shape = Waveform::Sine;
        NoiseGenerator noiseGen;
        float currentSNHValue = 0.0f;
    };

    //==============================================================================
    // Enhanced Envelope Follower (Based on Doc 2 1.3)
    //==============================================================================
    class EnvelopeFollower
    {
    public:
        // Added Curve parameter (0.0 = Linear, 1.0 = Logarithmic)
        void setCurve(float curveAmount) { curve = juce::jlimit(0.0f, 1.0f, curveAmount); }

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            juce::dsp::ProcessSpec monoSpec = spec;
            monoSpec.numChannels = 1;
            follower.prepare(monoSpec);
            setAttackTime(10.0f);
            setReleaseTime(100.0f);
        }

        void reset() { follower.reset(); }
        void setAttackTime(float attackMs) { follower.setAttackTime(attackMs); }
        void setReleaseTime(float releaseMs) { follower.setReleaseTime(releaseMs); }

        float process(float input)
        {
            float rectified = std::abs(input);

            // Apply curve shaping before smoothing (Doc 2 1.3)
            if (curve > 0.01f)
            {
                // Approximate log curve using a power function.
                // Exponent maps from 1.0 (Linear) down to ~0.3 (Logarithmic).
                float exponent = juce::jmap(curve, 1.0f, 0.3f);
                rectified = std::pow(rectified, exponent);
            }

            // FIX: BallisticsFilter requires the channel index (0 for mono usage).
            return follower.processSample(0, rectified);
        }

    private:
        juce::dsp::BallisticsFilter<float> follower;
        float curve = 0.0f;
    };
}