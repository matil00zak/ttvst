/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class PluginTestowy2AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    PluginTestowy2AudioProcessorEditor (PluginTestowy2AudioProcessor&);
    ~PluginTestowy2AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginTestowy2AudioProcessor& audioProcessor;
    juce::TextButton loadButton{ "Load File" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginTestowy2AudioProcessorEditor)
};
