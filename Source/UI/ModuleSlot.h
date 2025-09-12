#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CustomLookAndFeel.h"

class ModuleSelectionGrid;
class ModuleHeader;

class ModuleSlot : public juce::Component,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    ModuleSlot(juce::AudioProcessorValueTreeState& apvts, int slotIndex);
    ~ModuleSlot() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void createModule(int choice);
    // UPDATED: No longer returns an AudioProcessorEditor
    std::unique_ptr<juce::Component> createEditorForChoice(int choice);
    juce::String getModuleName(int choice);
    void showModuleMenu();

    juce::AudioProcessorValueTreeState& valueTreeState;
    int index;
    juce::String slotChoiceParamId;
    juce::String slotPrefix;

    CustomLookAndFeel lookAndFeel;

    std::unique_ptr<ModuleHeader> header;
    // UPDATED: No more temp processor, just a simple component for the editor
    std::unique_ptr<juce::Component> currentEditor;
    juce::TextButton addModuleButton{ "+" };

    juce::CallOutBox* callOut = nullptr;
};