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
//using LoadedPair = std::pair<std::shared_ptr<LoadedAudio>, std::shared_ptr<LoadedAudio>>;
struct Seg { int offset = 0; int value  = 0; };





void PluginTestowy2AudioProcessor::smoothRatios(std::vector<double>& ratios, double alpha)
{
    for (auto& r : ratios)
    {
        ratioLPState += alpha * (r - ratioLPState);
        r = ratioLPState;
    }
}


static std::shared_ptr<LoadedAudio>
loadFileIntoAudioBuffer(juce::AudioFormatManager& fm, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (!reader) return {};

    const int numChannels = (int)reader->numChannels;
    const juce::int64 numSamples64 = reader->lengthInSamples;
    if (numChannels <= 0 || numSamples64 <= 0) return {};

    const int numSamples = (int)numSamples64;

    auto out = std::make_shared<LoadedAudio>();
    out->sampleRate = (int)reader->sampleRate;
    out->buffer.setSize(numChannels, numSamples, false, false, true);

    const int block = 16384;
    juce::int64 filePos = 0;

    while (filePos < numSamples64)
    {
        const int toRead = (int)std::min<juce::int64>(block, numSamples64 - filePos);

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
LoadedAudioPtr PluginTestowy2AudioProcessor::getLoaded() const noexcept{
    // atomowy odczyt wskaznika (acquire para dla release w loaderze)
    return std::atomic_load_explicit(&loaded_, std::memory_order_acquire);
}


PluginTestowy2AudioProcessor::PluginTestowy2AudioProcessor()
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
    , apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();
}


PluginTestowy2AudioProcessor::~PluginTestowy2AudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginTestowy2AudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    // motor on off
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "motorOn",
        "Motor",
        false
    ));
    // pitch shift
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PitchShift",
        "Pitch Shift",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f)
    );

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TauTouch",
        "Tau Touch",
        juce::NormalisableRange<float>(0.0f, 0.2f, 0.001f), 0.04f)
    );
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TauFree",
        "Tau Free",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.01f), 0.5f)
    );

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "FilterOn",
        "Filter",
        true
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FilterBaseCutoff",
        "Base Cutoff",
        juce::NormalisableRange<float>(100.0f, 20000.0f, 100.0f), 16000.0f)
    );

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FilterAlpha",
        "Filter Alpha",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01), 1.0f)
    );


    return { params.begin(), params.end() };
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
    maxEventsPerBlock = 128;
    afterRenderOffsetVec.reserve(maxEventsPerBlock);
    afterRenderValueVec.reserve(maxEventsPerBlock);
    thisOffsetVec.reserve(maxEventsPerBlock);
    thisValueVec.reserve(maxEventsPerBlock);
    preRenderOffsetVec.reserve(maxEventsPerBlock);
    preRenderValueVec.reserve(maxEventsPerBlock);

    hostSampleRate_ = sampleRate;
    playhead_ = 0.0; // reset on (re)start
    setLatencySamples(samplesPerBlock);
    tau = 0.07;
    alpha = 1.0 - std::exp(-1.0 / (sampleRate * tau));
    splineCondition_.reset();

    lpfLeft.prepare(sampleRate);
    lpfRight.prepare(sampleRate);
    baseCutoff = 12000.0f;
    filterAlpha = 1.0;
    
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
    //DBG("beginLoadFile: " << file.getFullPathName());

    std::thread([this, file]
        {
            juce::AudioFormatManager fm;
            fm.registerBasicFormats(); // WAV/AIFF/FLAC/MP3* (MP3 depends on defines)

            auto data = loadFileIntoAudioBuffer(fm, file); // std::shared_ptr<LoadedAudio>
            if (data)
            {   
                //DBG("Loaded: " << file.getFileName()
                //    << "  SR=" << data->sampleRate
                //    << "  ch=" << data->buffer.getNumChannels()
                //    << "  samples=" << data->buffer.getNumSamples());

                // Publish as const to match the field type `std::shared_ptr<const LoadedAudio>`
                // NOTE: atomic_store/atomic_load overloads for shared_ptr are declared in <memory>.
                
                std::shared_ptr<const LoadedAudio> published = std::move(data);
                std::atomic_store_explicit(&loaded_, published, std::memory_order_release);

                //DBG("LOADEDD");
            }
            else
            {
                //DBG("Failed to load: " << file.getFullPathName());
            }
        }).detach();
}

double PluginTestowy2AudioProcessor::getPlayheadSeconds() const {
    return playhead_ / getSampleRate();
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

    const bool motorOn = apvts.getRawParameterValue("motorOn")->load();
    const bool filterOn = apvts.getRawParameterValue("FilterOn")->load();
    const float pitchShift = apvts.getRawParameterValue("PitchShift")->load();
    const float tauTouch = apvts.getRawParameterValue("TauTouch")->load();
    const float tauFree = apvts.getRawParameterValue("TauFree")->load();
    baseCutoff = apvts.getRawParameterValue("FilterBaseCutoff")->load();
    filterAlpha = apvts.getRawParameterValue("FilterAlpha")->load();




    //Snapshot loaded data
    auto data = getLoaded(); // later check if the loading data mechanism is allocation free
    if (!data) return;

    const int srcCh = data->buffer.getNumChannels();
    const int srcN = data->buffer.getNumSamples();
    const int outCh = buffer.getNumChannels();
    const int outN = buffer.getNumSamples();
    if (srcN <= 0) return;


    //IF HOST RESIZES BUFFER THEN DROP LAST BLOCK AND UPDATE LAST BLOCK SIZE
    if (lastBlock_.getNumChannels() != outCh || lastBlock_.getNumSamples() != outN) {
        lastBlock_.setSize(outCh, outN, false, true, true); // potential aloocation 
        //DBG("block resized");
        haveLastMidi_ = false;
    }


    //afterRenderOffsetCount = 0;
    //extractPitchWheelData(midiMessages, afterRenderOffsetCount, afterRenderOffsetVec.data(), afterRenderValueVec.data(), maxEventsPerBlock, outN);
    //if (afterRenderOffsetCount = 0) {
    //    afterRenderOffsetVec = {};
    //    afterRenderValueVec = {};
    //}
    //DBG(afterRenderOffsetCount);
    //
    auto optOff = getPitchWheelOffsetsVector(midiMessages);
    auto optVal = getPitchWheelValueVector(midiMessages);
    const bool havePitchThisBlock = (optOff && optVal);

    if (optOff && optVal) {
        auto ofs = optOff.value();
        std::transform(ofs.begin(), ofs.end(), ofs.begin(), [outN](float val) { return val + outN; });
        std::vector<double >values = pitchWheelToSamplePositionVec(*optVal);
        afterRenderOffsetVec = ofs;
        afterRenderValueVec = values;
        emptyBuffersCount = 0;
    }
    else {
        afterRenderOffsetVec = {};
        afterRenderValueVec = {};
        emptyBuffersCount++;
        //DBG("buffer empty");
        //count empty buffers
        
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



    const bool streamWasActive = splineCondition_.has_value(); // wczeœniej by³ ci¹g do interpolacji

    //// jeœli brak pitch w tym bloku, a wczeœniej stream dzia³a³: patchuj values_
    //if (emptyBuffersCount > 0 && emptyBuffersCount <= midiDropoutToleranceBlocks_ && streamWasActive)
    //{
    //    //alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_ * tauTouch));
    //    //ratios_.assign(outN, ratioLPState);   // sample&hold ostatniej prêdkoœci
    //    // NIE resetuj splineCondition_, lastSpline, splineSet_
    //    //DBG("patched empty buffer no " << emptyBuffersCount);
    //    double last_delta_t = offsets_[offsets_.size() - 1] - offsets_[offsets_.size() - 2];
    //    double last_delta_pos = values_[values_.size() - 1] - values_[values_.size() - 2];
    //    double last_speed = last_delta_pos / last_delta_t;
    //    double delta_t = outN - offsets_[offsets_.size() - 1];
    //    double synthetic_pos = values_[values_.size() - 1] + delta_t * last_speed;
    //    afterRenderOffsetVec.push_back(outN);
    //    afterRenderValueVec.push_back(synthetic_pos);
    //    values_.push_back(synthetic_pos);
    //    offsets_.push_back(outN);
    //    DBG("inserted speed");
    //}


    ttvst::helps::vectorPairDbl speedinfo = positionsToSpeed(values_, offsets_, outN, 2);
    speeds_ = speedinfo.first;
    speed_offsets_ = speedinfo.second;



    catchSpeedOutliers(speeds_, 6.0);

    // if the interpolation stream just starts, insert the last generated speed (form smoothed speed)
    if (!splineCondition_.has_value()) {
        insertBaseSpeed(speeds_, speed_offsets_, ratioLPState);
    }
    //else if (emptyBuffersCount < 3) {
    //    insertLastSpeed(speeds_, speed_offsets_, lastSpeed, lastOffset, outN);
    //}
    // else
    // if the stram gets interrupted (splineCondition.has_value() && emptyBufferCount < 3)
    // insert last speed (or a prediction)
    // then if emptyBufferCount == 2 do nothing --> no spline condition will be generated ->> free state, motor steering
    // lastSpeed --> must be in the next msg --> offset > outN
    // 

    if (speeds_.size() > 1) {
        //tau = 0.04;
        alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_ * tauTouch));
        ratios_ = {};
        splineSetPlus splineSetPlus_ = splineSpecial(speed_offsets_, speeds_, splineCondition_, 1, outN);
        splineSet_ = splineSetPlus_.set;
        splineCondition_ = splineSetPlus_.spline_condition;
        //DBG("alpha: " << splineCondition_->alpha);

        if (lastSpline.x < 0) {
            splineSet_.insert(splineSet_.begin(), lastSpline);
        }

        lastSpline = splineSetPlus_.jointSpline;
        lastSpline.x = lastSpline.x - outN;

        ratios_ = createSpeedVector(splineSet_, outN);
        //DBG("ratios size: " << ratios_.size());

    }
    else {
        //DBG("processBlock: no messages to create vector from");
        //tau = 0.5;
        alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_ * tauFree));
        splineSet_ = {};
        lastSpline = {};
        splineCondition_.reset();
        if (motorOn) {

            ratios_.assign(outN, 1.0 + (1.0 * pitchShift / 12.0));
        }
        else {
            ratios_.assign(outN, 0.0);
        }
    }
    


    if (ratios_.size() == outN) {
        //smoothing only here
        smoothRatios(ratios_, alpha);
        //append_vector_csv("ratios_smo_2048_006.csv", ratios_, 6);
        //linear
        /*
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
        */
        //hermite
        //looped playback
        float cutofff;
        for (int i = 0; i < outN; i++)
        {   
            wrapPlayhead(playhead_, srcN);
            const long index1 = (long)playhead_;
            const long index0 = (index1 - 1 + srcN) % srcN;
            const long index2 = (index1 + 1) % srcN;
            const long index3 = (index1 + 2) % srcN;

            const double frac = playhead_ - (double)index1;
            const double frac2 = frac * frac;
            const double frac3 = frac2 * frac;

            const float speedAbs = std::abs(ratios_[i]);

            const float cutoff = baseCutoff * std::pow(speedAbs, filterAlpha);
            
            const float cutoffClamped = juce::jlimit(50.0f, 0.45f * (float)hostSampleRate_, cutoff);
            


            for (int ch = 0; ch < outCh; ch++)
            {
                const float y0 = *data->buffer.getReadPointer(ch, index0);
                const float y1 = *data->buffer.getReadPointer(ch, index1);
                const float y2 = *data->buffer.getReadPointer(ch, index2);
                const float y3 = *data->buffer.getReadPointer(ch, index3);

                // 4-point cubic Hermite (Catmull-Rom)
                const double a0 = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
                const double a1 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
                const double a2 = -0.5 * y0 + 0.5 * y2;
                const double a3 = y1;

                float out = (float)(a0 * frac3 + a1 * frac2 + a2 * frac + a3);

                if (filterOn) {
                    if (ch == 0)
                        out = lpfLeft.processSample(out, cutoffClamped);
                    else
                        out = lpfRight.processSample(out, cutoffClamped);
                }



                buffer.setSample(ch, i, out);
            }
            cutofff = cutoffClamped;
            playhead_ += ratios_[i];
        }
        
    }
    else {
        //DBG("THIS CASE SHOULD NOT EVER EXECUTE AND SHOULD BE DELETED SOON!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        for (int i = 0; i < outN; i++) {
            wrapPlayhead(playhead_, srcN);
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
