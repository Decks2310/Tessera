//================================================================================
// File: DSPUtils.h (Restored + compatibility shims)
//================================================================================
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <random>
#include <array>
#include <cmath>

namespace DSPUtils
{
    //==============================================================================
    // Fast Math Approximations
    //==============================================================================
    inline float fastTanh(float x)
    {
        if (x > 4.97f) return 1.0f;
        if (x < -4.97f) return -1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Fast approximation of sin(2*pi*x). Input x is phase [0,1)
    inline double fastSinCycle(double x)
    {
        x = x - std::floor(x);
        const double PI = juce::MathConstants<double>::pi;
        const double TWO_PI = juce::MathConstants<double>::twoPi;
        double t = x * TWO_PI - PI;
        const double B = 4.0 / PI;
        const double C = -4.0 / (PI * PI);
        double y = B * t + C * t * std::abs(t);
        const double P = 0.225;
        y = P * (y * std::abs(y) - y) + y;
        return y;
    }

    //==============================================================================
    // NoiseGenerator
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
                return distribution(randomEngine);
            float white = distribution(randomEngine);
            pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
            pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
            pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
            pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
            pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
            pinkState[5] = -0.7616f * pinkState[5] - white * 0.0168980f;
            float pink = pinkState[0] + pinkState[1] + pinkState[2] + pinkState[3] + pinkState[4] + pinkState[5] + pinkState[6] + white * 0.5362f;
            pinkState[6] = white * 0.115926f;
            return pink * 0.11f;
        }

        // Compatibility shim (legacy code may call nextFloat())
        float nextFloat() { return getNextSample(); }

    private:
        NoiseType type = NoiseType::White;
        std::minstd_rand randomEngine;
        std::uniform_real_distribution<float> distribution;
        std::array<float, 7> pinkState{};
    };

    //==============================================================================
    // LFO
    //==============================================================================
    class LFO
    {
    public:
        enum class Waveform { Sine, Triangle, Saw, Square, SampleAndHold };

        void prepare(const juce::dsp::ProcessSpec& spec) { sampleRate = spec.sampleRate; reset(); }
        void reset() { phase = 0.0; currentSNHValue = noiseGen.getNextSample(); }
        void setFrequency(float freqHz) { phaseIncrement = (sampleRate > 0.0) ? (double)freqHz / sampleRate : 0.0; }
        void setWaveform(Waveform newShape) { shape = newShape; }
        void setStereoOffset(float offset) { stereoOffset = juce::jlimit(0.0, 1.0, (double)offset); }

        std::pair<float, float> getNextStereoSample()
        {
            if (shape == Waveform::SampleAndHold && phase < phaseIncrement)
                currentSNHValue = noiseGen.getNextSample();
            float left = (float)generateWaveform(phase);
            double rPhase = std::fmod(phase + stereoOffset, 1.0);
            float right = (float)generateWaveform(rPhase);
            phase += phaseIncrement; if (phase >= 1.0) phase -= 1.0; return { left, right };
        }
        float getNextBipolar() { return getNextStereoSample().first; }
        float getNextUnipolar() { return (getNextStereoSample().first + 1.0f) * 0.5f; }

    private:
        double generateWaveform(double p)
        {
            switch (shape)
            {
            case Waveform::Sine: return fastSinCycle(p);
            case Waveform::Triangle: return (p < 0.5) ? 4.0 * p - 1.0 : -4.0 * p + 3.0;
            case Waveform::Saw: return 2.0 * p - 1.0;
            case Waveform::Square: return (p < 0.5) ? 1.0 : -1.0;
            case Waveform::SampleAndHold: return currentSNHValue;
            default: return 0.0;
            }
        }
        double sampleRate = 44100.0, phase = 0.0, phaseIncrement = 0.0, stereoOffset = 0.0; Waveform shape = Waveform::Sine; NoiseGenerator noiseGen; float currentSNHValue = 0.0f;
    };

    //==============================================================================
    // EnvelopeFollower (with compatibility shims)
    //==============================================================================
    class EnvelopeFollower
    {
    public:
        void setCurve(float curveAmount) { curve = juce::jlimit(0.0f, 1.0f, curveAmount); }

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            juce::dsp::ProcessSpec monoSpec = spec; monoSpec.numChannels = 1; follower.prepare(monoSpec); setAttackTime(10.0f); setReleaseTime(100.0f);
        }
        // Legacy signature (ignored float param)
        void prepare(double sampleRate, float) { juce::dsp::ProcessSpec spec{ sampleRate, 512, 1 }; prepare(spec); }

        void reset() { follower.reset(); }
        void setAttackTime(float a) { follower.setAttackTime(a); }
        void setReleaseTime(float r) { follower.setReleaseTime(r); }

        float process(float input)
        {
            float rectified = std::abs(input);
            if (curve > 0.01f)
            {
                float exponent = juce::jmap(curve, 1.0f, 0.3f);
                rectified = std::pow(rectified, exponent);
            }
            return follower.processSample(0, rectified);
        }
        // Legacy name mapping
        float processSample(int /*channel*/, float input) { return process(input); }

    private:
        juce::dsp::BallisticsFilter<float> follower; float curve = 0.0f;
    };
}