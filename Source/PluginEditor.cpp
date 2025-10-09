/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginTestowy2AudioProcessorEditor::PluginTestowy2AudioProcessorEditor (PluginTestowy2AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

 // Set the callback for when the button is clicked

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this]()
        {
            DBG("CLICKED");
            fileChooser = std::make_unique<juce::FileChooser>(
                "Select an audio file...", juce::File{},
                "*.wav;*.aiff;*.flac;*.mp3"
            );

            auto flags = juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles;

            fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                        audioProcessor.beginLoadFile(file); // Twój stub/loader

                    fileChooser.reset(); // posprz¹taj po dialogu
                });
        };


    // MIDI monitor setup
    midiMonitor.setMultiLine(true);
    midiMonitor.setReadOnly(true);
    midiMonitor.setScrollbarsShown(true);
    midiMonitor.setCaretVisible(false);
    midiMonitor.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(midiMonitor);
    
    startTimerHz(30); // poll MIDI log ~30 FPS



    setSize (400, 300);
}

PluginTestowy2AudioProcessorEditor::~PluginTestowy2AudioProcessorEditor()
{

}

//==============================================================================
void PluginTestowy2AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
    //loadButton.setBounds(getLocalBounds().reduced(20));
}

void PluginTestowy2AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    loadButton.setBounds(getLocalBounds().reduced(20));
    auto area = getLocalBounds().reduced(8);
    auto top = area.removeFromTop(36);
    loadButton.setBounds(top.removeFromLeft(140));
    area.removeFromTop(8);
    midiMonitor.setBounds(area);

}

 void PluginTestowy2AudioProcessorEditor::timerCallback()
 {
    std::vector<ttvst::MidiEvent> events;
    audioProcessor.getMidiLog().drainTo(events);
    
    if (events.empty()) return;
    
    // Append new lines to our fixed-size buffer
    for (const auto& e : events)
        midiLines.add(e.toString());
    
    // Trim to last kMaxLines
    if (midiLines.size() > kMaxLines)
        midiLines.removeRange(0, midiLines.size() - kMaxLines);
    
    // Re-render (small list, so full rewrite is fine)
    midiMonitor.setText(midiLines.joinIntoString("\n"), false);
    midiMonitor.moveCaretToEnd();
 }
