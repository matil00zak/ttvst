/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginTestowy2AudioProcessorEditor::PluginTestowy2AudioProcessorEditor (PluginTestowy2AudioProcessor& p)
    : AudioProcessorEditor (&p),
    audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

 // Set the callback for when the button is clicked
    if (auto f = audioProcessor.getLastLoadedFile(); f.existsAsFile())
        thumbnail.setSource(new juce::FileInputSource(f));

    setResizable(true, false);
    getConstrainer()->setFixedAspectRatio(1.5);

    auto setupKnob = [](juce::Slider& s)
        {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        };
    

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
                        audioProcessor.beginLoadFile(file);
                        audioProcessor.setLastLoadedFile(file);
                        
                    thumbnail.setSource(new juce::FileInputSource(file));
                    
                    

                    fileChooser.reset();
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

    addAndMakeVisible(tempoModeButton);
    tempoModeButton.setButtonText("Tempo Mode");
    tempoModeAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.getAPVTS(),
        "TempoMode",
        tempoModeButton
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
    tauTouchSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 32);
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
    
    addAndMakeVisible(scratchScaleSlider);
    scratchScaleSlider.setSliderStyle(juce::Slider::Rotary);
    scratchScaleSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 32, 32);
    scratchScaleAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.getAPVTS(),
        "ScratchScale",
        scratchScaleSlider
    );

    // MIDI monitor setup
    midiMonitor.setMultiLine(true);
    midiMonitor.setReadOnly(true);
    midiMonitor.setScrollbarsShown(true);
    midiMonitor.setCaretVisible(false);
    midiMonitor.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(midiMonitor);
    


    setupKnob(tauFreeSlider);
    setupKnob(tauTouchSlider);
    setupKnob(scratchScaleSlider);

    startTimerHz(60); // poll MIDI log ~30 FPS
    setSize (600, 400);

    thumbnailFormatManager.registerBasicFormats();
}

PluginTestowy2AudioProcessorEditor::~PluginTestowy2AudioProcessorEditor()
{

}

//==============================================================================
void PluginTestowy2AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(juce::Colour(56, 56, 56));

    const float radius = 12.0f;
    g.setColour(juce::Colour(71, 62, 91));
    g.fillRoundedRectangle(areaBottom.toFloat(), radius);
    g.fillRoundedRectangle(areaTopA.toFloat(), radius);
    g.fillRoundedRectangle(areaTopB.toFloat(), radius);
    g.fillRoundedRectangle(areaTopC.toFloat(), radius);

    g.setColour(juce::Colour(56, 56, 56));
    g.fillRoundedRectangle(contentB.toFloat(), radius);

    g.setColour(juce::Colour(56, 56, 56));
    g.fillRoundedRectangle(waveformArea.toFloat(), 12.0f);

    constexpr double visibleWindowSecons = 0.5;
    
    const double visualLatencySeconds = processor.getBlockSize() / audioProcessor.getFileSR();

    if (thumbnail.getNumChannels() > 0)
    {
        double playhead = audioProcessor.getPlayheadSeconds();
        playhead -= visualLatencySeconds;
        const double halfWindow = visibleWindowSeconds * 0.5;

        double startTime = playhead - halfWindow;
        double endTime = playhead + halfWindow;

        startTime = juce::jlimit(0.0, thumbnail.getTotalLength(), startTime);
        endTime = juce::jlimit(0.0, thumbnail.getTotalLength(), endTime);

        // Background
        g.setColour(juce::Colour(56, 56, 56));
        g.fillRoundedRectangle(waveformArea.toFloat(), 12.0f);

        // Waveform (scrolls)
        g.setColour(juce::Colour(228, 216, 107));
        thumbnail.drawChannels(g, waveformArea, startTime, endTime, 1.0f);
        
        // Fixed center playhead
        const int playheadX = waveformArea.getX() + waveformArea.getWidth() / 2;

        g.setColour(juce::Colours::red);
        g.fillRect(playheadX - 1,
            waveformArea.getY(),
            2,
            waveformArea.getHeight());
    }
}
void PluginTestowy2AudioProcessorEditor::resized() {
    const int itemH = 60; // knob + textbox
    int m = 3;
    int mT = 2;
    auto height = getHeight();
    auto width = getWidth();
    auto r = getLocalBounds().reduced(m);
    areaBottom = r.removeFromBottom(width / 5).reduced(m);
    areaTop = r.reduced(m);

    int totalW = areaTop.getWidth();
    const int w1 = 2, w2 = 5, w3 = 2;
    const int sum = w1 + w2 + w3;
    const int aW = (totalW * w1) / sum;
    const int bW = (totalW * w2) / sum;
    const int cW = totalW - aW - bW;

    areaTopA = areaTop.removeFromLeft(aW).reduced(m);
    areaTopB = areaTop.removeFromLeft(bW).reduced(m);
    areaTopC = areaTop.reduced(m); // what's left

    //blob.setBounds(areaTopC.reduced(10));
    contentA = areaTopA.reduced(12);   // padding inside the rounded rect

    const int n = 3;
    //const int itemH = 28;                  // slider height
    const int totalItemsH = n * itemH;

    const int gap = (contentA.getHeight() - totalItemsH) / (n + 1); // spaceEvenly
    int y = contentA.getY() + gap;

    auto place = [&](juce::Component& c)
        {
            c.setBounds(contentA.getX(), y, contentA.getWidth(), itemH);
            y += itemH + gap;
        };

    place(tauFreeSlider);
    place(tauTouchSlider);
    place(scratchScaleSlider);
    //place(filterAlphaSlider);



    // center a vertical slider in areaTopC with proportional height
    contentC = areaTopC.reduced(12);

    constexpr float heightRatio = 0.80f;          // 80% of areaTopC height (change as needed)
    const int sliderH = (int)std::round(contentC.getHeight() * heightRatio);

    const int sliderW = juce::jmin(40, contentC.getWidth()); // pick a width you like
    auto s4Bounds = juce::Rectangle<int>(0, 0, sliderW, sliderH)
        .withCentre(contentC.getCentre());

    pitchShiftSlider.setBounds(s4Bounds);
    auto cont = areaTopB;
    contentB = cont.removeFromBottom(areaTopB.getHeight() / 3).reduced(12);
    contentD = areaBottom.reduced(12);
    waveformArea = contentD;

    {
        auto inner = contentB.reduced(8);

        const int n = 3;
        const int buttonH = juce::jmin(28, inner.getHeight());
        const int y = inner.getY() + (inner.getHeight() - buttonH) / 2;

        // space-evenly: equal space left, between, right
        int gap = 8;
        int buttonW = (inner.getWidth() - gap * (n + 1)) / n;

        // safety if the area gets small
        if (buttonW < 40)
        {
            gap = 4;
            buttonW = juce::jmax(40, (inner.getWidth() - gap * (n + 1)) / n);
        }

        int x = inner.getX() + gap;

        loadButton.setBounds(x, y, buttonW, buttonH); x += buttonW + gap;
        motorButton.setBounds(x, y, buttonW, buttonH); x += buttonW + gap;
        tempoModeButton.setBounds(x, y, buttonW, buttonH);
    }

}

//FIRST GUI
//void PluginTestowy2AudioProcessorEditor::resized()
//{
//    auto area = getLocalBounds().reduced(10); // margin around edges
//
//    waveformArea = area.removeFromTop(100);
//
//    // Reserve bottom area for MIDI monitor
//    int midiHeight = 80;
//    auto midiArea = area.removeFromBottom(midiHeight);
//    midiMonitor.setBounds(midiArea);
//
//    // Top row: buttons (load, clear, motor, filter)
//    int buttonHeight = 30;
//    int buttonSpacing = 10;
//    auto buttonArea = area.removeFromTop(buttonHeight);
//
//    int buttonCount = 5;
//    int buttonWidth = (buttonArea.getWidth() - (buttonCount - 1) * buttonSpacing) / buttonCount;
//
//    loadButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
//    buttonArea.removeFromLeft(buttonSpacing);
//
//    clearLogButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
//    buttonArea.removeFromLeft(buttonSpacing);
//
//    motorButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
//    buttonArea.removeFromLeft(buttonSpacing);
//
//    filterButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
//    buttonArea.removeFromLeft(buttonSpacing);
//
//    tempoModeButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
//
//    // Sliders row: evenly spaced horizontally in remaining area
//    int sliderCount = 5; // pitchShift, tauTouch, tauFree, filterBaseCutoff, filterAlpha
//    int sliderSpacing = 20;
//    int sliderWidth = (area.getWidth() - sliderSpacing * (sliderCount - 1)) / sliderCount;
//    int sliderHeight = sliderWidth; // square sliders for rotary style
//    int topY = area.getY() + (area.getHeight() - sliderHeight) / 2; // vertically centered
//
//    pitchShiftSlider.setBounds(0, 0, 0, 0); // just to avoid warnings
//    tauTouchSlider.setBounds(0, 0, 0, 0);
//    tauFreeSlider.setBounds(0, 0, 0, 0);
//    filterBaseCutoffSlider.setBounds(0, 0, 0, 0);
//    filterAlphaSlider.setBounds(0, 0, 0, 0);
//
//    int x = area.getX();
//    pitchShiftSlider.setBounds(x, topY, sliderWidth, sliderHeight);
//    x += sliderWidth + sliderSpacing;
//    tauTouchSlider.setBounds(x, topY, sliderWidth, sliderHeight);
//    x += sliderWidth + sliderSpacing;
//    tauFreeSlider.setBounds(x, topY, sliderWidth, sliderHeight);
//    x += sliderWidth + sliderSpacing;
//    filterBaseCutoffSlider.setBounds(x, topY, sliderWidth, sliderHeight);
//    x += sliderWidth + sliderSpacing;
//    filterAlphaSlider.setBounds(x, topY, sliderWidth, sliderHeight);
//}

 void PluginTestowy2AudioProcessorEditor::timerCallback()
 {

    repaint();
    //std::vector<ttvst::MidiEvent> events;
    //audioProcessor.getMidiLog().drainTo(events);
    //
    //if (events.empty()) return;
    //
    //// Append new lines to our fixed-size buffer
    //for (const auto& e : events)
    //    midiLines.add(e.toString());
    //
    //// Trim to last kMaxLines
    //if (midiLines.size() > kMaxLines)
    //    midiLines.removeRange(0, midiLines.size() - kMaxLines);
    //
    //// Re-render (small list, so full rewrite is fine)
    //midiMonitor.setText(midiLines.joinIntoString("\n"), false);
    //midiMonitor.moveCaretToEnd();
 }
