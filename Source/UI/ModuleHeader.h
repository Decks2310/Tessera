#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ModuleHeader : public juce::Component,
    public juce::DragAndDropTarget
{
public:
    std::function<void()> onMenuClicked;
    std::function<void()> onDeleteClicked;
    std::function<int()> getSlotIndex;
    std::function<void(int, int)> onSlotMoved;

    juce::Label title;
    juce::TextButton optionsButton{ "..." };
    juce::TextButton deleteButton{ "-" };

    ModuleHeader();

    void paint(juce::Graphics& g) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    // Drag and Drop Target overrides
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;

    void resized() override;
private:
    bool isDragHovering = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleHeader)
};