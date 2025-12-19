/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>
#include <cstring> // memcpy (gdyby bylo potrzebne w innych wariantach)
#include <juce_audio_formats/codecs/flac/format.h>
#include <pluginterfaces/base/ftypes.h>
#include <wtypes.h>
#include <cmath>
#include "helpers.h"
#include "cubicSplines.h"
//==============================================================================
using LoadedPair = std::pair<std::shared_ptr<LoadedAudio>, std::shared_ptr<LoadedAudio>>;
struct Seg { int offset = 0; int value  = 0; };


void PluginTestowy2AudioProcessor::smoothRatios(std::vector<double>& ratios, double alpha)
{
    for (auto& r : ratios)
    {
        ratioLPState += alpha * (r - ratioLPState);
        r = ratioLPState;
    }
}

int PluginTestowy2AudioProcessor::getDeltaPh(int start, int end, int hostSr) {
    const int delta = end - start;                 // can be negative
    const double frac = static_cast<double>(delta) / 16383.0;   // 14-bit range
    const double seconds = 2.0;
    const double samples = frac * (static_cast<double>(hostSr) * seconds);
    return static_cast<int>(std::lround(samples));
}


int PluginTestowy2AudioProcessor::renderSeg(LoadedAudioPtr srcAudio,
    juce::AudioSampleBuffer outBuffer,
    juce::LagrangeInterpolator interp,
    double ratio,
    int lenIn,
    int lenOut,
    int numCh){
    const float* src = nullptr;
    int realDelta = 0;
    for (int ch = 0; ch < numCh; ch++){
        src = srcAudio->buffer.getReadPointer(ch, playhead_);
        float* out = outBuffer.getWritePointer(ch, 0);
        realDelta = interp.process(ratio, src, out, lenOut, lenIn, 0);
        interp.reset();
    }
    return realDelta;
}


static LoadedPair
loadFileIntoAudioBuffer(juce::AudioFormatManager& fm, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (!reader) return {};

    const int numChannels = (int)reader->numChannels;
    const juce::int64 numSamples64 = reader->lengthInSamples;
    if (numChannels <= 0 || numSamples64 <= 0) return {};

    // Uwaga: AudioBuffer rozmiar w int – rzutujemy swiadomie (typowo pliki < 2 31 probek)
    const int numSamples = (int)numSamples64;

    auto out = std::make_shared<LoadedAudio>();
    out->sampleRate = (int)reader->sampleRate;
    auto outReversed = std::make_shared<LoadedAudio>();
    out->sampleRate = (int)reader->sampleRate;
    outReversed->buffer.setSize(numChannels, numSamples, false, false, true);
    out->buffer.setSize(numChannels, numSamples, false, false, true);
    // setSize(ch, samples, keepContent=false, clearExtraSpace=false, avoidReallocating=true)

    // Czytamy partiami, by wspierac bardzo dlugie pliki
    const int block = 16384;
    juce::int64 filePos = 0;

    while (filePos < numSamples64)
    {
        const int toRead = (int)std::min<juce::int64>(block, numSamples64 - filePos);

        // Czytamy bezposrednio do bufora docelowego (destStart = (int)filePos)
        // Flagi true/true odnosza sis do L/R przy plikach stereo — przy mono/wiecej kanalow
        // JUCE i tak wypelni dostepne kanaly; to najczestszy przypadek (1–2 ch).
        if (!reader->read(&out->buffer,
            (int)filePos,            // destStartSample
            toRead,                   // numSamples
            filePos,                  // start w pliku
            true, true))              // (left/right for stereo)
            break;

        filePos += toRead;
    }
    outReversed->buffer.makeCopyOf(out->buffer);
    outReversed->buffer.reverse(0, numSamples);

    return { out, outReversed };
}
LoadedAudioPtr PluginTestowy2AudioProcessor::getLoaded() const noexcept{
    // atomowy odczyt wskaznika (acquire para dla release w loaderze)
    return std::atomic_load_explicit(&loaded_, std::memory_order_acquire);
}
LoadedAudioPtr PluginTestowy2AudioProcessor::getLoadedReversed() const noexcept {
    // atomowy odczyt wskaznika (acquire para dla release w loaderze)
    return std::atomic_load_explicit(&loadedReversed_, std::memory_order_acquire);
}

PluginTestowy2AudioProcessor::PluginTestowy2AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

PluginTestowy2AudioProcessor::~PluginTestowy2AudioProcessor()
{
}

//==============================================================================
const juce::String PluginTestowy2AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginTestowy2AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginTestowy2AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginTestowy2AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginTestowy2AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginTestowy2AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginTestowy2AudioProcessor::getCurrentProgram()
{
    return 0;
}

void PluginTestowy2AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PluginTestowy2AudioProcessor::getProgramName (int index)
{
    return {};
}

void PluginTestowy2AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}


//==============================================================================
void PluginTestowy2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    hostSampleRate_ = sampleRate;
    playhead_ = 0.0; // reset on (re)start
    //playheadReversed_ = 0;
    setLatencySamples(samplesPerBlock);
    tau = 0.05;
    alpha = 1.0 - std::exp(-1.0 / (sampleRate * tau));
    
}

void PluginTestowy2AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PluginTestowy2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PluginTestowy2AudioProcessor::beginLoadFile(const juce::File& file)
{
    DBG("beginLoadFile: " << file.getFullPathName());

    std::thread([this, file]
        {
            juce::AudioFormatManager fm;
            fm.registerBasicFormats(); // WAV/AIFF/FLAC/MP3* (MP3 depends on defines)

            auto[ data, dataReversed ] = loadFileIntoAudioBuffer(fm, file); // std::shared_ptr<LoadedAudio>
            if (data && dataReversed)
            {   
                DBG("Loaded: " << file.getFileName()
                    << "  SR=" << data->sampleRate
                    << "  ch=" << data->buffer.getNumChannels()
                    << "  samples=" << data->buffer.getNumSamples());

                // Publish as const to match the field type `std::shared_ptr<const LoadedAudio>`
                // NOTE: atomic_store/atomic_load overloads for shared_ptr are declared in <memory>.
                if (data->getNumSamples() == dataReversed->getNumSamples()) {
                    std::shared_ptr<const LoadedAudio> published = std::move(data);
                    std::atomic_store_explicit(&loaded_, published, std::memory_order_release);
                    std::shared_ptr<const LoadedAudio> publishedReversed = std::move(dataReversed);
                    std::atomic_store_explicit(&loadedReversed_, publishedReversed, std::memory_order_release);
                }
                else {
                    DBG("data and its reversed version have unexpected differences");
                }
                DBG("LOADEDD");
            }
            else
            {
                DBG("Failed to load: " << file.getFullPathName());
            }
        }).detach();
}

void PluginTestowy2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    using namespace ttvst::helps;
    using namespace ttvst::splines;
    juce::ScopedNoDenormals _;
    juce::MidiBuffer midiThisBlock = midiMessages;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Capture MIDI for the UI
    for (const auto metadata : midiMessages)
        midiLog_.pushFromAudioThread(metadata.getMessage(), metadata.samplePosition);

    buffer.clear();
    offsets_ = {};
    values_ = {};
    ratios_ = {};
    speeds_ = {};
    speed_offsets_ = {};

    //Snapshot loaded data
    auto data = getLoaded();
    auto dataReversed = getLoadedReversed();
    if (!data || !dataReversed) return;
    const int srcCh = data->buffer.getNumChannels();
    const int srcN = data->buffer.getNumSamples();
    const int outCh = buffer.getNumChannels();
    const int outN = buffer.getNumSamples();
    if (srcN <= 0) return;

    if (hasPitchWheelMessage(lastMidi_)) {
        DBG("proessBlock: lastMIDI has msgs");
    }
    else {
        DBG("processBlock: lastMIDI no msgs");
    }


    //IF HOST RESIZES BUFFER THEN DROP LAST BLOCK AND UPDATE LAST BLOCK SIZE
    if (lastBlock_.getNumChannels() != outCh || lastBlock_.getNumSamples() != outN) {
        lastBlock_.setSize(outCh, outN, false, true, true);
        DBG("block resized");
        haveLastMidi_ = false;
    }

    //get midi
    auto optOff = getPitchWheelOffsetsVector(midiMessages);
    auto optVal = getPitchWheelValueVector(midiMessages);
    if (optOff && optVal) {
        auto ofs = optOff.value();
        std::transform(ofs.begin(), ofs.end(), ofs.begin(), [outN](float val) { return val + outN; });
        std::vector<double >values = pitchWheelToSamplePositionVec(*optVal);
        afterRenderOffsetVec = ofs;
        afterRenderValueVec = values;
    }
    else {
        afterRenderOffsetVec = {};
        afterRenderValueVec = {};
    }

    
    // creating vectors for messages in index range: < - outN, outN > -- moving in out memory buffers
    offsets_.insert(offsets_.end(), preRenderOffsetVec.begin(), preRenderOffsetVec.end());
    offsets_.insert(offsets_.end(), thisOffsetVec.begin(), thisOffsetVec.end());
    offsets_.insert(offsets_.end(), afterRenderOffsetVec.begin(), afterRenderOffsetVec.end());

    values_.insert(values_.end(), preRenderValueVec.begin(), preRenderValueVec.end());
    values_.insert(values_.end(), thisValueVec.begin(), thisValueVec.end());
    values_.insert(values_.end(), afterRenderValueVec.begin(), afterRenderValueVec.end());

    // getting desired messages from in out buffer
    // version with calculating speed between messages, needed only one message from lookahead 
    // also a last current-lookahead shared spline is needed to generate full buffer

    ttvst::helps::vectorPairDbl speedinfo = positionsToSpeed(values_, offsets_, outN, 2);
    speeds_ = speedinfo.first;
    speed_offsets_ = speedinfo.second;
    for (auto ofs : speed_offsets_) {
        DBG(ofs);
    }


    if (speeds_.size() > 2) {
        ratios_ = {};
        splineSetPlus splineSetPlus_ = splineSpecial(speed_offsets_, speeds_, splineCondition_, 1);
        splineSet_ = splineSetPlus_.set;
        splineCondition_ = splineSetPlus_.spline_condition;
        DBG(splineCondition_->alpha);

        if (lastSpline.x < 0) {
            splineSet_.insert(splineSet_.begin(), lastSpline);
        }

        lastSpline = splineSetPlus_.set[splineSetPlus_.set.size()-1-1];
        lastSpline.x = lastSpline.x - outN;

        splineSet_.pop_back();

        ratios_ = createSpeedVector(splineSet_, outN);
        ratios_ = createSpeedVector(splineSet_, outN);
        //append_vector_csv("ratios_reg_002.csv", ratios_, 6);
        smoothRatios(ratios_, alpha);
        DBG("ratios size: " << ratios_.size());
        //append_vector_csv("ratios_smo_002.csv", ratios_, 6);
    }
    else {
        DBG("processBlock: no messages to create vector from");
        DBG("processBlock: pre render values reset");
        splineSet_ = {};
        lastSpline = {};
        splineCondition_.reset();
        ratios_.assign(outN, 1.0);
        ratioLPState - 1.0;
    }


    if (ratios_.size() == outN) {
        for (int i = 0; i < outN; i++) {
            auto index0 = (unsigned long)playhead_;
            auto index1 = index0 == (srcN - 1) ? (unsigned int)0 : index0 + 1;
            auto frac = playhead_ - (double)index0;
            for (int ch = 0; ch < outCh; ch++) {
                auto value0 = *data->buffer.getReadPointer(ch, index0);
                auto value1 = *data->buffer.getReadPointer(ch, index1);
                auto currentSample = value0 + frac * (value1 - value0);
                buffer.setSample(ch, i, (float)currentSample);
            }
            playhead_ += ratios_[i];
        }
    }
    else {
        for (int i = 0; i < outN; i++) {
            auto index0 = (unsigned long)playhead_;
            for (int ch = 0; ch < outCh; ch++) {
                float value = *data->buffer.getReadPointer(0, index0);
                buffer.setSample(ch, i, value);
            }
            playhead_ += 1;
        }
    }


    
    // preparing the message vectors for the next buffer and update, so when it comes everything is in the desired range
    // relative 0 at the middle buffer start
    std::transform(afterRenderOffsetVec.begin(), afterRenderOffsetVec.end(), afterRenderOffsetVec.begin(),
        [outN](float val) { return val - outN; });
    
    std::transform(thisOffsetVec.begin(), thisOffsetVec.end(), thisOffsetVec.begin(),
        [outN](float val) { return val - outN; });


    preRenderValueVec = thisValueVec;
    preRenderOffsetVec = thisOffsetVec;
    thisValueVec = afterRenderValueVec;
    thisOffsetVec = afterRenderOffsetVec;


    if (hasPitchWheelMessage(midiMessages)) {
        lastMidi_.swapWith(midiMessages);
    }
    else {
        lastMidi_.clear();
    }

}

//==============================================================================
bool PluginTestowy2AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginTestowy2AudioProcessor::createEditor()
{
    return new PluginTestowy2AudioProcessorEditor (*this);
}

//==============================================================================
void PluginTestowy2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void PluginTestowy2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginTestowy2AudioProcessor();
}
