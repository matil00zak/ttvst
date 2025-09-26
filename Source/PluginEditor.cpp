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
    loadButton.setBounds(getLocalBounds().reduced(20));
}

void PluginTestowy2AudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
