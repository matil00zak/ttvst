/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once    

#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <vector>
#include "LoadedAudio.h"
#include "MidiMessageManager.h"
#include "helpers.h"

//==============================================================================
/**
*/



class PluginTestowy2AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PluginTestowy2AudioProcessor();
    ~PluginTestowy2AudioProcessor() override;


    ttvst::MidiMessageManager& getMidiLog() noexcept { return midiLog_; }

    std::shared_ptr<const LoadedAudio> getLoaded() const noexcept;
    std::shared_ptr<const LoadedAudio> getLoadedReversed() const noexcept;
    void beginLoadFile(const juce::File& file);
    int getDeltaPh(int endVal, int startVal, int hostSr);
    void setMotorState(bool state);

    int renderSeg(LoadedAudioPtr srcAudio,
        juce::AudioSampleBuffer outBuffer,
        juce::LagrangeInterpolator interp,
        double ratio,
        int lenIn,
        int lenOut,
        int numCh);
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;



private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginTestowy2AudioProcessor)
    std::shared_ptr<const LoadedAudio> loaded_, loadedReversed_;
    ttvst::MidiMessageManager midiLog_;
    std::optional<int> lastOffset, afterRenderOffset, preRenderOffset;
    std::optional<double> lastValue, afterRenderValue, preRenderValue;
    std::vector<double> offsets_, values_, ratios_;
    enum block { pre, render, after };
    double hostSampleRate_ = 44100.0;  // set in prepareToPlay
    //int64_t playhead_ = 0;
    //int64_t playheadReversed_ = 0;// current read position in source samples
    double playhead_ = 0;
    double I_sim = 0.0;
    double I_ext = 0.0;
    double beta = 0.0;
    bool motorState = false;
    bool loop_ = true;
    bool afterRender = false;
    bool preRender = false;
    juce::AudioBuffer<float> lastBlock_;
    juce::MidiBuffer lastMidi_;
    bool haveLastMidi_ = false;
    bool haveLast_ = false;
    juce::LinearInterpolator interp;

};
