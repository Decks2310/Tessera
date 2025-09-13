#pragma once
#include <JuceHeader.h>
#include "UIConfig.h"

class OrbController : public juce::Component, public juce::Slider::Listener, public juce::Timer
{
public:
    juce::Slider tuneSlider; // Y axis
    juce::Slider mixSlider;  // X axis

    OrbController()
    {
        configureSlider(tuneSlider);
        configureSlider(mixSlider);
        currentMixNormalised = (float)mixSlider.valueToProportionOfLength(mixSlider.getValue());
        currentTuneNormalised = (float)tuneSlider.valueToProportionOfLength(tuneSlider.getValue());
        startTimerHz(60);
    }

    void paint(juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        // Padding so orb not clipped
        auto area = full.reduced(full.getWidth() * 0.15f, full.getHeight() * 0.15f);

        // Map normalized to position within area
        float x = area.getX() + currentMixNormalised * area.getWidth();
        float y = area.getBottom() - currentTuneNormalised * area.getHeight(); // invert Y

        float pulsation = 0.05f * currentMixNormalised * std::sin(phase);
        float baseRadius = juce::jmin(area.getWidth(), area.getHeight()) * 0.18f;
        float radius = baseRadius * (1.0f + pulsation);

        juce::Colour primaryColour = juce::Colours::yellow;
#ifdef UIConfig_h
        primaryColour = UIConfig::PRIMARY_COLOUR;
#endif

        juce::ColourGradient gradient(primaryColour.withAlpha(juce::jlimit(0.2f, 0.8f, currentMixNormalised * 1.2f)), x, y,
            primaryColour.withAlpha(0.0f), x, y + radius * 2.0f, true);
        g.setGradientFill(gradient);
        g.fillEllipse(x - radius * 2.0f, y - radius * 2.0f, radius * 4.0f, radius * 4.0f);

        g.setColour(primaryColour.withAlpha(0.9f));
        g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        g.drawText("Drag: Tune (Y) / Mix (X)", full.removeFromBottom(18), juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        isDragging = true;
        startDragNormalisedValue.x = (float)mixSlider.valueToProportionOfLength(mixSlider.getValue());
        startDragNormalisedValue.y = (float)tuneSlider.valueToProportionOfLength(tuneSlider.getValue());
        mixSlider.startedDragging();
        tuneSlider.startedDragging();
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (isDragging)
        {
            mixSlider.stoppedDragging();
            tuneSlider.stoppedDragging();
        }
        isDragging = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!isDragging) return;
        auto delta = event.getOffsetFromDragStart();
        float sensitivity = 1.0f / 180.0f; // slightly faster
        float newMixNorm = juce::jlimit(0.0f, 1.0f, startDragNormalisedValue.x + delta.x * sensitivity);
        float newTuneNorm = juce::jlimit(0.0f, 1.0f, startDragNormalisedValue.y - delta.y * sensitivity);
        double newMixValue = mixSlider.proportionOfLengthToValue(newMixNorm);
        double newTuneValue = tuneSlider.proportionOfLengthToValue(newTuneNorm);
        mixSlider.setValue(newMixValue, juce::sendNotification);
        tuneSlider.setValue(newTuneValue, juce::sendNotification);
    }

    void sliderValueChanged(juce::Slider* slider) override
    {
        if (slider == &mixSlider)
            currentMixNormalised = (float)mixSlider.valueToProportionOfLength(mixSlider.getValue());
        else if (slider == &tuneSlider)
            currentTuneNormalised = (float)tuneSlider.valueToProportionOfLength(tuneSlider.getValue());
        repaint();
    }

    void timerCallback() override
    {
        phase += 0.08f; if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi; repaint();
    }
private:
    float currentMixNormalised = 0.5f;
    float currentTuneNormalised = 0.5f;
    float phase = 0.0f;
    juce::Point<float> startDragNormalisedValue;
    bool isDragging = false;

    void configureSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.addListener(this);
        addAndMakeVisible(slider);
        slider.setVisible(false);
    }
};