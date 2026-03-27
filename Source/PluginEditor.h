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
    juce::ToggleButton tempoModeButton;
    juce::Slider pitchShiftSlider;
    juce::Slider tauTouchSlider;
    juce::Slider tauFreeSlider;
    juce::ToggleButton filterButton;
    juce::Slider filterAlphaSlider;
    juce::Slider scratchScaleSlider;
    juce::ComboBox vinylRpmBox;
    juce::ComboBox motorRpmBox;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> motorAttachment;
    std::unique_ptr<ButtonAttachment> filterAttachment;
    std::unique_ptr<ButtonAttachment> tempoModeAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> pitchShiftAttachment;
    std::unique_ptr<SliderAttachment> scratchScaleAttachment;
    std::unique_ptr<SliderAttachment> tauTouchAttachment;
    std::unique_ptr<SliderAttachment> tauFreeAttachment;
    std::unique_ptr<SliderAttachment> filterBaseCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterAlphaAttachment;
    juce::TextButton loadButton{ "Load File" };
    juce::TextButton clearLogButton{ "Clear Logs" };
    juce::TextEditor midiMonitor;
    juce::Rectangle<int> waveformArea;
    juce::StringArray midiLines;
    static constexpr int kMaxLines = 100;
    std::unique_ptr<juce::FileChooser> fileChooser;
    double visibleWindowSeconds = 0.5;
    juce::AudioFormatManager thumbnailFormatManager;
    juce::AudioThumbnailCache thumbnailCache{ 5 };
    juce::AudioThumbnail thumbnail{16, thumbnailFormatManager, thumbnailCache};
    juce::Rectangle<int> areaTop, areaBottom, areaTopA, areaTopB, areaTopC, contentA, contentB, contentC, contentD;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginTestowy2AudioProcessorEditor)
};
