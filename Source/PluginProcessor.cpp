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
//==============================================================================


struct Seg { int start = 0; int end = 0; int guwno; };
static std::shared_ptr<LoadedAudio>

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

    return out;
}
std::shared_ptr<const LoadedAudio>
PluginTestowy2AudioProcessor::getLoaded() const noexcept
{
    // atomowy odczyt wskaznika (acquire para dla release w loaderze)
    return std::atomic_load_explicit(&loaded_, std::memory_order_acquire);
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
    float playhead_ = 0.0; // reset on (re)start
    setLatencySamples(samplesPerBlock);

    
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

            auto data = loadFileIntoAudioBuffer(fm, file); // std::shared_ptr<LoadedAudio>
            if (data)
            {
                DBG("Loaded: " << file.getFileName()
                    << "  SR=" << data->sampleRate
                    << "  ch=" << data->buffer.getNumChannels()
                    << "  samples=" << data->buffer.getNumSamples());

                // Publish as const to match the field type `std::shared_ptr<const LoadedAudio>`
                // NOTE: atomic_store/atomic_load overloads for shared_ptr are declared in <memory>.
                std::shared_ptr<const LoadedAudio> published = std::move(data);
                std::atomic_store_explicit(&loaded_, published, std::memory_order_release);
            }
            else
            {
                DBG("Failed to load: " << file.getFullPathName());
            }
        }).detach();
}

void PluginTestowy2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{   
    std::optional<int> preRenderOffset, lastOffset, afterRenderOffset;
    std::optional<int> preRenderValue, lastValue, afterRenderValue;
    std::optional<Seg> afterRenderSeg;
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
    if (!data) return;
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
            Seg afterRenderSeg = { *afterRenderOffset, outN, *afterRenderValue };
            break;
        }
    }


    if (haveLastMidi_) {
        int countPB = 0;
        for (const auto metadata : lastMidi_) {
            const auto& m = metadata.getMessage();
            if (!m.isPitchWheel()) continue;
            const int offset = metadata.samplePosition;
            int val = m.getPitchWheelValue();
            if (segs.isEmpty()) {
                segs.add({ 0, offset, val });
                
            }
            else {
                //float playheadMovement = (val-*lastValue) / 0.75 * hostSampleRate_ / 16000;
                segs.add({*lastOffset , offset, val });
                lastOffset = offset;
                lastValue = val;
            }
            ++countPB;
            lastOffset = offset;
            lastValue = val;//later save value into prerender
        }
        DBG("PB count:" << countPB);
    }

    if (preRenderOffset) {
        segs.insert(0, { *preRenderOffset, outN, *preRenderValue });
        DBG("seg inserted");
    }
    /*if (afterRenderOffset.has_value()) {
        segs.add(*afterRenderSeg);

    }*/

    preRenderOffset = lastOffset;
    preRenderValue = lastValue;
    

    for (const auto seg : segs) {
        DBG("start:" << seg.start << "stop:" << seg.end << "value:" << seg.guwno);
    }
    
    buffer.clear();

    if (haveLast_){
        for (int ch = 0; ch < outCh; ++ch)
            buffer.copyFrom(ch, 0, lastBlock_, ch, 0, outN);
        
    }

    for (int ch = 0; ch < outCh; ++ch)
        lastBlock_.copyFrom(ch, 0, render_, ch, 0, outN);
    

    if (haveLastMidi_) {
        midiMessages.swapWith(lastMidi_);
        for (const auto metadata : lastMidi_) {
            const auto m = metadata.getMessage();
            if (m.isPitchWheel()) {
                preRenderOffset = metadata.samplePosition;
                preRenderValue = m.getPitchWheelValue();
                continue;
            }
        }
    }
    lastMidi_.swapWith(midiThisBlock);


    haveLastMidi_ = true;
    haveLast_ = true;
}
    //simple playback
    /*
    for (int n = 0; n < outN; ++n)
    {
        // End behavior: loop or clamp
        if (playhead_ >= srcN)
        {
            if (loop_) playhead_ = 0;
            else break; // leave silence for the rest of the block
        }

        // Read one sample from source (mix to mono if stereo)
        float s = 0.0f;
        if (srcCh == 1)
        {
            s = data->buffer.getSample(0, (int)playhead_);
        }
        else
        {
            // simple L/R average; later you can copy channels 1:1
            const float l = data->buffer.getSample(0, (int)playhead_);
            const float r = data->buffer.getSample(1, (int)playhead_);
            s = 0.5f * (l + r);
        }

        // Write the sample to all output channels
        for (int ch = 0; ch < outCh; ++ch)
            buffer.setSample(ch, n, s);

        ++playhead_; // advance 1 sample (1× speed)
    }
}
*/
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
