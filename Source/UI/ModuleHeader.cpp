#include "ModuleHeader.h"
#include "CustomLookAndFeel.h"

ModuleHeader::ModuleHeader()
{
    addAndMakeVisible(title);
    title.setJustificationType(juce::Justification::centred);
    title.setFont(juce::FontOptions(16.0f));

    addAndMakeVisible(optionsButton);
    optionsButton.onClick = [this] { if (onMenuClicked) onMenuClicked(); };

    addAndMakeVisible(deleteButton);
    deleteButton.onClick = [this] { if (onDeleteClicked) onDeleteClicked(); };
}

void ModuleHeader::paint(juce::Graphics& g)
{
    if (isDragHovering)
    {
        CustomLookAndFeel laf;
        g.setColour(laf.accentColour);
        g.drawRect(getLocalBounds().toFloat(), 2.0f);
    }
}

void ModuleHeader::mouseDrag(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (dragContainer)
    {
        juce::var dragDescription = getSlotIndex();
        // Create a dummy 1x1 image
        juce::Image dragImage(juce::Image::PixelFormat::ARGB, 1, 1, true);

        // FIX: Wrap the image in juce::ScaledImage to use the non-deprecated overload.
        dragContainer->startDragging(dragDescription, this, juce::ScaledImage(dragImage), false);
    }
}

bool ModuleHeader::isInterestedInDragSource(const SourceDetails&)
{
    return true;
}

void ModuleHeader::itemDropped(const SourceDetails& dragSourceDetails)
{
    int sourceSlotIndex = dragSourceDetails.description;
    int targetSlotIndex = getSlotIndex();
    if (onSlotMoved)
        onSlotMoved(sourceSlotIndex, targetSlotIndex);

    isDragHovering = false;
    repaint();
}

void ModuleHeader::itemDragEnter(const SourceDetails&)
{
    isDragHovering = true;
    repaint();
}

void ModuleHeader::itemDragExit(const SourceDetails&)
{
    isDragHovering = false;
    repaint();
}

void ModuleHeader::resized()
{
    auto bounds = getLocalBounds();
    deleteButton.setBounds(bounds.removeFromLeft(30).reduced(5));
    optionsButton.setBounds(bounds.removeFromRight(30).reduced(5));
    title.setBounds(bounds);
}