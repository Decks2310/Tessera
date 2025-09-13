#include "ParameterUIs.h"
#include <JuceHeader.h> // Also needed here!

ParameterTextBox::ParameterTextBox(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& pId, const juce::String& lText)
    : apvts(apvtsRef), paramId(pId), labelText(lText)
{
    parameter = apvts.getParameter(paramId);
    apvts.addParameterListener(paramId, this);
    startTimerHz(30);
}

ParameterTextBox::~ParameterTextBox()
{
    apvts.removeParameterListener(paramId, this);
}

void ParameterTextBox::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto labelBounds = bounds.removeFromLeft(bounds.getWidth() / 2);
    g.setColour(lookAndFeel.textColour);
    g.drawFittedText(labelText, labelBounds, juce::Justification::centredLeft, 1);

    if (parameter)
    {
        lookAndFeel.drawTextBoxedText(g, parameter->getCurrentValueAsText(), bounds);
    }
}

void ParameterTextBox::resized()
{
}

void ParameterTextBox::parameterChanged(const juce::String&, float)
{
}