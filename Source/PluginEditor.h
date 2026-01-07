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
    , private juce::Timer
{
public:
    PluginTestowy2AudioProcessorEditor (PluginTestowy2AudioProcessor&);
    ~PluginTestowy2AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    juce::GroupComponent controlsGroup;
    juce::GroupComponent plateGroup;

    PluginTestowy2AudioProcessor& audioProcessor;
    juce::ToggleButton motorButton;
    juce::Slider pitchShiftSlider;
    juce::Slider tauTouchSlider;
    juce::Slider tauFreeSlider;
    juce::ToggleButton filterButton;
    juce::Slider filterBaseCutoffSlider;
    juce::Slider filterAlphaSlider;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> motorAttachment;
    std::unique_ptr<ButtonAttachment> filterAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> pitchShiftAttachment;
    std::unique_ptr<SliderAttachment> tauTouchAttachment;
    std::unique_ptr<SliderAttachment> tauFreeAttachment;
    std::unique_ptr<SliderAttachment> filterBaseCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterAlphaAttachment;
    juce::TextButton loadButton{ "Load File" };
    juce::TextButton clearLogButton{ "Clear Logs" };
    juce::TextEditor midiMonitor;
    // Keep only a fixed number of recent lines to avoid UI slowdown
    juce::StringArray midiLines;
    static constexpr int kMaxLines = 100; // tweak as you like
    std::unique_ptr<juce::FileChooser> fileChooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginTestowy2AudioProcessorEditor)
};
