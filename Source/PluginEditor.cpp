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
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(1.5);


    
    addAndMakeVisible(loadButton);
    loadButton.onClick = [this](){
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

    addAndMakeVisible(clearLogButton);
    clearLogButton.onClick = [this]() {
        midiMonitor.clear();
        };

    addAndMakeVisible(motorButton);
    motorButton.setButtonText("Motor");
    motorAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.getAPVTS(),
        "motorOn",
        motorButton
    );

    addAndMakeVisible(filterButton);
    filterButton.setButtonText("Filter");
    filterAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.getAPVTS(),
        "FilterOn",
        filterButton
    );

    addAndMakeVisible(pitchShiftSlider);
    pitchShiftSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchShiftSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 32, 32);
    pitchShiftAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.getAPVTS(),
        "PitchShift",
        pitchShiftSlider
    );


    addAndMakeVisible(tauTouchSlider);
    tauTouchSlider.setSliderStyle(juce::Slider::Rotary);
    tauTouchSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 32, 32);
    tauTouchAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.getAPVTS(),
        "TauTouch",
        tauTouchSlider
    );

    addAndMakeVisible(tauFreeSlider);
    tauFreeSlider.setSliderStyle(juce::Slider::Rotary);
    tauFreeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 32, 32);
    tauFreeAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.getAPVTS(),
        "TauFree",
        tauFreeSlider
    );

    addAndMakeVisible(filterBaseCutoffSlider);
    filterBaseCutoffSlider.setSliderStyle(juce::Slider::Rotary);
    filterBaseCutoffSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 32, 32);
    filterBaseCutoffAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.getAPVTS(),
        "FilterBaseCutoff",
        filterBaseCutoffSlider
    );

    addAndMakeVisible(filterAlphaSlider);
    filterAlphaSlider.setSliderStyle(juce::Slider::Rotary);
    filterAlphaSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 32, 32);
    filterAlphaAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.getAPVTS(),
        "FilterAlpha",
        filterAlphaSlider
    );


    // MIDI monitor setup
    midiMonitor.setMultiLine(true);
    midiMonitor.setReadOnly(true);
    midiMonitor.setScrollbarsShown(true);
    midiMonitor.setCaretVisible(false);
    midiMonitor.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(midiMonitor);
    
    startTimerHz(10); // poll MIDI log ~30 FPS
    setSize (600, 400);
}

PluginTestowy2AudioProcessorEditor::~PluginTestowy2AudioProcessorEditor()
{

}

//==============================================================================
void PluginTestowy2AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black.brighter(0.2));

    g.setColour (juce::Colours::whitesmoke);
    g.setFont (juce::FontOptions (15.0f));
}

void PluginTestowy2AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10); // margin around edges

    // Reserve bottom area for MIDI monitor
    int midiHeight = 80;
    auto midiArea = area.removeFromBottom(midiHeight);
    midiMonitor.setBounds(midiArea);

    // Top row: buttons (load, clear, motor, filter)
    int buttonHeight = 30;
    int buttonSpacing = 10;
    auto buttonArea = area.removeFromTop(buttonHeight);

    int buttonWidth = (buttonArea.getWidth() - 3 * buttonSpacing) / 4; // 4 buttons

    loadButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    clearLogButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    motorButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(buttonSpacing);
    filterButton.setBounds(buttonArea.removeFromLeft(buttonWidth));

    // Sliders row: evenly spaced horizontally in remaining area
    int sliderCount = 5; // pitchShift, tauTouch, tauFree, filterBaseCutoff, filterAlpha
    int sliderSpacing = 20;
    int sliderWidth = (area.getWidth() - sliderSpacing * (sliderCount - 1)) / sliderCount;
    int sliderHeight = sliderWidth; // square sliders for rotary style
    int topY = area.getY() + (area.getHeight() - sliderHeight) / 2; // vertically centered

    pitchShiftSlider.setBounds(0, 0, 0, 0); // just to avoid warnings
    tauTouchSlider.setBounds(0, 0, 0, 0);
    tauFreeSlider.setBounds(0, 0, 0, 0);
    filterBaseCutoffSlider.setBounds(0, 0, 0, 0);
    filterAlphaSlider.setBounds(0, 0, 0, 0);

    int x = area.getX();
    pitchShiftSlider.setBounds(x, topY, sliderWidth, sliderHeight);
    x += sliderWidth + sliderSpacing;
    tauTouchSlider.setBounds(x, topY, sliderWidth, sliderHeight);
    x += sliderWidth + sliderSpacing;
    tauFreeSlider.setBounds(x, topY, sliderWidth, sliderHeight);
    x += sliderWidth + sliderSpacing;
    filterBaseCutoffSlider.setBounds(x, topY, sliderWidth, sliderHeight);
    x += sliderWidth + sliderSpacing;
    filterAlphaSlider.setBounds(x, topY, sliderWidth, sliderHeight);
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
