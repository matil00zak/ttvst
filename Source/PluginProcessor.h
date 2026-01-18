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
#include "cubicSplines.h"
#include "CascadedOnePoleLPF.h"

//==============================================================================
/**
*/



class PluginTestowy2AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PluginTestowy2AudioProcessor();
    ~PluginTestowy2AudioProcessor() override;
    void PluginTestowy2AudioProcessor::smoothRatiosS(std::vector<double>& ratios, double alpha);
    void PluginTestowy2AudioProcessor::smoothRatios(std::vector<double>& ratios, double alpha);

    double getPlayheadSeconds() const;
    int getFileSR() const;
    ttvst::MidiMessageManager& getMidiLog() noexcept { return midiLog_; }
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    std::shared_ptr<const LoadedAudio> getLoaded() const noexcept;
    //std::shared_ptr<const LoadedAudio> getLoadedReversed() const noexcept;
    void beginLoadFile(const juce::File& file);

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

    void setLastLoadedFile(const juce::File& f) { lastLoadedFile = f; }
    juce::File getLastLoadedFile() const { return lastLoadedFile; }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTestowy2AudioProcessor)
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::shared_ptr<const LoadedAudio> loaded_;                                     // loaded audio ptr
    ttvst::MidiMessageManager midiLog_;                                             // logs container
    std::vector<double> offsets_, values_;                                          // contains all avilable msgs data, just storage, dbg
    std::vector<double> speeds_, speed_offsets_, ratios_;                           // crucial very important data, base for generation
    double lastSpeed, lastOffset;                                                   // data for the empty buffers // patches MIDI stream interruption
    int emptyBuffersCount;
    double hostSampleRate_;
    double playhead_ = 0.0;                                                         // the playhead position
    std::vector<double> thisValueVec, afterRenderValueVec, preRenderValueVec;       // message data stream containers
    std::vector<double> thisOffsetVec, afterRenderOffsetVec, preRenderOffsetVec;    // message data stream containers
    std::vector<ttvst::splines::splineSet> splineSet_;  // deleted: lastSplines     // set of splines (generated every iteration)
    std::optional<ttvst::splines::splineCondition> splineCondition_;                // condition passed between spline set generation
    ttvst::splines::splineSet lastSpline;                                           // last spline container - completes the spline set
    double ratioLPState = 0.0; 
    double ratioLPStateStage1_ = 0.0;// speed inertia base // speed inertia container
    double tau, alpha;
    //std::atomic<int> debugEvent{ 0 };
    // inertia parameters

    juce::AudioBuffer<float> lastBlock_;                                            // should be used to detect buffer size change. to do.
    juce::MidiBuffer lastMidi_;                                                     // midi messages contariner. in use
    bool haveLastMidi_ = false;                                                     // old functionality. 
    int afterRenderOffsetCount = 0;
    int maxEventsPerBlock;

    //filters
    CascadedOnePoleLPF lpfLeft;
    CascadedOnePoleLPF lpfRight;
    float baseCutoff = 10000.0f;
    float filterAlpha = 0.5f;
    double tauFreeMotor = 0.5;
    bool  touchDown_ = false;       // CC64 >=64
    int   pitchEmptyStreak_ = 0;    // kolejne bloki bez pitch wheel
    double lastGoodSpeed_ = 0.0;    // ostatnia sensowna prêdkoœæ (ratio)

    double playheadOnTouchdown_;
    int bufferID;
    juce::AudioFormatManager formatManager;
    juce::File lastLoadedFile;
    int fileSR = 5;
    std::vector<float> lut;

    
};


