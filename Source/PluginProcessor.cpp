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
//==============================================================================
using LoadedPair = std::pair<std::shared_ptr<LoadedAudio>, std::shared_ptr<LoadedAudio>>;
struct Seg { int offset = 0; int value  = 0; };




int PluginTestowy2AudioProcessor::getDeltaPh(int start, int end, int hostSr) {
    const int delta = end - start;                 // can be negative
    const double ratio = static_cast<double>(delta) / 16383.0;   // 14-bit range
    const double seconds = 4.0 / 3.0;
    const double samples = ratio * (static_cast<double>(hostSr) * seconds);
    return static_cast<int>(std::lround(samples));
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
    playhead_ = 0; // reset on (re)start
    playheadReversed_ = 0;
    setLatencySamples(samplesPerBlock);
    preRenderOffset = 0;
    preRenderValue = 0;
    
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
    juce::ScopedNoDenormals _;
    juce::MidiBuffer midiThisBlock = midiMessages;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Capture MIDI for the UI
    
    for (const auto metadata : midiMessages)
        midiLog_.pushFromAudioThread(metadata.getMessage(), metadata.samplePosition);

    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());


    // Snapshot the loaded data (atomic load in your getLoaded())
    auto data = getLoaded();
    auto dataReversed = getLoadedReversed();
    if (!data || !dataReversed) return;
    const int srcCh = data->buffer.getNumChannels();
    const int srcN = data->buffer.getNumSamples();

    const int outCh = buffer.getNumChannels();
    const int outN = buffer.getNumSamples();

    if (srcN <= 0) return;

    //if host rezized buffer then haveLast = false
    if (lastBlock_.getNumChannels() != outCh || lastBlock_.getNumSamples() != outN){
        lastBlock_.setSize(outCh, outN, false, true, true);
        render_.setSize(outCh, outN, false, true, true);
        haveLast_ = false; 
        haveLastMidi_ = false;
    }

    render_.clear();
    //modify the render
    //struct Seg { int start = 0; int end = 0; int guwno; };
    juce::Array<Seg, juce::CriticalSection, 16> segs;    

    for (const auto metadata : midiMessages) {
        const auto& m = metadata.getMessage();
        if (m.isPitchWheel()) {
            afterRenderOffset = metadata.samplePosition;
            afterRenderValue = m.getPitchWheelValue();
            break;
        }
    }

    int countPB = 0;
    if (haveLastMidi_) {
        
        for (const auto metadata : lastMidi_) {
            const auto& m = metadata.getMessage();
            if (!m.isPitchWheel()) continue;
            const int offset = metadata.samplePosition;
            const int val = m.getPitchWheelValue();
            if (segs.isEmpty()) {
                segs.add({ offset, val});
            }
            else {
                //float playheadMovement = (val-*lastValue) / 0.75 * hostSampleRate_ / 16000;
                if (lastOffset) {
                    segs.add({ offset, val });
                }
            }
            ++countPB;
            lastOffset = offset;
            lastValue = val;
        }
        DBG("PB count:" << countPB);
    }

    if (preRenderOffset && preRenderValue) {
        Seg preRenderSeg{*preRenderOffset, *preRenderValue};
        DBG("afterRenderSeg created");
    }

    if (afterRenderOffset) {
        Seg afterRenderSeg{*afterRenderOffset, *afterRenderValue,};
        DBG("preRenderSeg created");
    }

    //RENDERING
    
    if (haveLastMidi_ && !segs.isEmpty()) {
        
        const float* inBuffer = nullptr;

        //render 0 - midiMessage[0]
        if (preRenderOffset.has_value() && preRenderValue.has_value()) {
            int lenOut = segs[0].offset + 1;
            int lenIn = std::abs(getDeltaPh(segs[0].value, *preRenderValue, hostSampleRate_));
            if (lenOut <= 0 || lenIn <= 0) return; // or continue / set ratio=1
            double ratio = static_cast<double>(lenIn) / lenOut;
            if (getDeltaPh(segs[0].value, *preRenderValue, hostSampleRate_) < 0) {
                inBuffer = dataReversed->buffer.getReadPointer(0, srcN - 1 - playhead_);
            }
            else {
                inBuffer = data->buffer.getReadPointer(0, playhead_);
            }
            float* outBuffer = render_.getWritePointer(0, 0);
            int realDelta = interp.process(ratio, inBuffer, outBuffer, lenOut, lenIn, 0);
            realDelta *= (realDelta < 0) ? -1 : 1;
            playhead_ += realDelta;
        }
        
        //render midiMessage[0] - midiMessage[last]
        for (size_t i = 0, n = segs.size(); i + 1 < n; ++i) {
            const Seg& seg = segs[i];
            const Seg& next = segs[i + 1];
            int lenOut = next.value - seg.value;
            int deltaPh = (getDeltaPh(next.value, seg.value, hostSampleRate_));
            int lenIn = std::abs(deltaPh);
            if (lenOut <= 0 || lenIn <= 0) return; // or continue / set ratio=1
            double ratio = static_cast<double>(lenIn) / lenOut;
            if (deltaPh < 0) {
                inBuffer = dataReversed->buffer.getReadPointer(0, srcN - 1 - playhead_);
                
            }
            else {
                inBuffer = data->buffer.getReadPointer(0, playhead_);
            }
            float* outBuffer = render_.getWritePointer(0, 0);
            int realDelta = interp.process(ratio, inBuffer, outBuffer, lenOut, lenIn, 0);
            
            realDelta *= (realDelta < 0) ? -1 : 1;
            playhead_ += realDelta;
        }

        if (afterRenderOffset.has_value() && afterRenderValue.has_value()) {
            const Seg& lastSeg = *segs.end();
            int lenOut = render_.getNumSamples() - 1 - lastSeg.offset;
            int deltaPh = (getDeltaPh(*afterRenderValue, lastSeg.value, hostSampleRate_));
            int lenIn = std::abs(deltaPh);
            if (lenOut <= 0 || lenIn <= 0) return; // or continue / set ratio=1
            double ratio = static_cast<double>(lenIn) / lenOut;
            if (deltaPh < 0) {
                inBuffer = dataReversed->buffer.getReadPointer(0, srcN - 1 - playhead_);
            }
            else {
                inBuffer = data->buffer.getReadPointer(0, playhead_);
            }
            float* outBuffer = render_.getWritePointer(0, 0);
            int realDelta = interp.process(ratio, inBuffer, outBuffer, lenOut, lenIn, 0);

            realDelta *= (realDelta < 0) ? -1 : 1;
            playhead_ += realDelta;

        }



    }
    


    
    if (lastOffset && lastValue) {
        preRenderOffset = *lastOffset;
        preRenderValue = *lastValue;
    }
    else {
        preRenderOffset.reset();
        preRenderValue.reset();
    }
    afterRenderOffset.reset();
    afterRenderValue.reset();
    lastOffset.reset();
    lastValue.reset();


    for (const auto seg : segs) {
        DBG("stop:" << seg.offset << "value:" << seg.value);
    }
    
    buffer.clear();

    /*if (haveLast_) {
        for (int ch = 0; ch < outCh; ++ch)
            buffer.copyFrom(ch, 0, lastBlock_, ch, 0, outN);
        
    }*/

    for (int ch = 0; ch < outCh; ++ch)
        buffer.copyFrom(ch, 0, render_, ch, 0, outN);
    

    if (haveLastMidi_) {
        midiMessages.swapWith(lastMidi_);
    }
    lastMidi_.swapWith(midiThisBlock);


    haveLastMidi_ = true;
    haveLast_ = true;
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
