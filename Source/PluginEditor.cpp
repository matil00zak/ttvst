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

    if (auto f = audioProcessor.getLastLoadedFile(); f.existsAsFile())
        thumbnail.setSource(new juce::FileInputSource(f));

    setResizable(true, false);
    getConstrainer()->setFixedAspectRatio(1.5);

    auto setupKnob = [](juce::Slider& s){
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

        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc){
                auto file = fc.getResult();
                if (file.existsAsFile())
                    audioProcessor.beginLoadFile(file);
                    audioProcessor.setLastLoadedFile(file);
                        
                thumbnail.setSource(new juce::FileInputSource(file));
                    
                    

                fileChooser.reset();
        });
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
    vinylRpmBox.addItem("33 1/3", 1);
    vinylRpmBox.addItem("45", 2);
    addAndMakeVisible(vinylRpmBox);

    motorRpmBox.addItem("33 1/3", 1);
    motorRpmBox.addItem("45", 2);
    addAndMakeVisible(motorRpmBox);


    setupKnob(tauFreeSlider);
    setupKnob(tauTouchSlider);
    setupKnob(scratchScaleSlider);

    startTimerHz(60);
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


        g.setColour(juce::Colour(56, 56, 56));
        g.fillRoundedRectangle(waveformArea.toFloat(), 12.0f);


        g.setColour(juce::Colour(228, 216, 107));
        thumbnail.drawChannels(g, waveformArea, startTime, endTime, 1.0f);
        

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
    contentA = areaTopA.reduced(12);

    const int n = 3;
    const int totalItemsH = n * itemH;

    const int gap = (contentA.getHeight() - totalItemsH) / (n + 1);
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




    contentC = areaTopC.reduced(12);

    constexpr float heightRatio = 0.80f;
    const int sliderH = (int)std::round(contentC.getHeight() * heightRatio);

    const int sliderW = juce::jmin(40, contentC.getWidth());
    auto s4Bounds = juce::Rectangle<int>(0, 0, sliderW, sliderH)
        .withCentre(contentC.getCentre());

    pitchShiftSlider.setBounds(s4Bounds);
    auto cont = areaTopB;
    contentB = cont.removeFromBottom(areaTopB.getHeight() / 3).reduced(12);
    contentD = areaBottom.reduced(12);
    waveformArea = contentD;


    {
        auto inner = contentB.reduced(8);

        const int n = 2;

        auto topRow = inner.removeFromTop(inner.getHeight() * 0.5f);
        auto bottomRow = inner;
        {
            const int buttonH = juce::jmin(28, topRow.getHeight());
            const int y = topRow.getY() + (topRow.getHeight() - buttonH) / 2;

            int gap = 8;
            int buttonW = (topRow.getWidth() - gap * (n + 1)) / n;

            if (buttonW < 40)
            {
                gap = 4;
                buttonW = juce::jmax(40, (topRow.getWidth() - gap * (n + 1)) / n);
            }

            int x = topRow.getX() + gap;

            loadButton.setBounds(x, y, buttonW, buttonH); x += buttonW + gap;
            motorButton.setBounds(x, y, buttonW, buttonH);
        }

        {
            const int boxH = juce::jmin(28, bottomRow.getHeight());
            const int y = bottomRow.getY() + (bottomRow.getHeight() - boxH) / 2;

            int gap = 8;
            int boxW = (bottomRow.getWidth() - gap * (n + 1)) / n;

            if (boxW < 40)
            {
                gap = 4;
                boxW = juce::jmax(40, (bottomRow.getWidth() - gap * (n + 1)) / n);
            }

            int x = bottomRow.getX() + gap;

            vinylRpmBox.setBounds(x, y, boxW, boxH); x += boxW + gap;
            motorRpmBox.setBounds(x, y, boxW, boxH);
        }
    }


}


 void PluginTestowy2AudioProcessorEditor::timerCallback()
 {

      repaint();

 }
