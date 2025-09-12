#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "CustomLookAndFeel.h"

// Renamed from IconTextButton and updated for SVG rendering
class ModuleGridButton : public juce::Button
{
public:
    // Updated constructor to take SVG data (const char*)
    ModuleGridButton(const juce::String& text, const char* svgData);
    // Updated paintButton signature
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
private:
    // Use juce::Drawable for SVG
    std::unique_ptr<juce::Drawable> svgImage;
    // CustomLookAndFeel lookAndFeel; // <-- REMOVE THIS LINE
};

class ModuleSelectionGrid : public juce::Component
{
public:
    std::function<void(int)> onModuleSelected;

    ModuleSelectionGrid(juce::StringArray choices);
    ~ModuleSelectionGrid() override; // Added destructor

    // Added paint override for the grid background
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    // Helper function updated to return SVG data
    const char* getSvgDataForChoice(int choice);
    // Updated array type
    juce::OwnedArray<ModuleGridButton> buttons;
    CustomLookAndFeel lookAndFeel;
};