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
#include "LutSincInterpolation.h"
#include "WavLogger.cpp"
//==============================================================================
//using LoadedPair = std::pair<std::shared_ptr<LoadedAudio>, std::shared_ptr<LoadedAudio>>;
struct Seg { int offset = 0; int value  = 0; };



std::vector<double> PluginTestowy2AudioProcessor::linearContinuationFromLastSlope(const std::vector<double>& in,
    std::size_t numOut)
{
    std::vector<double> out;
    out.reserve(numOut);

    if (numOut == 0) return out;

    // Not enough data to estimate a slope -> default slope = 0 (flat continuation).
    if (in.empty()) {
        out.assign(numOut, 0.0);
        return out;
    }
    if (in.size() == 1) {
        out.assign(numOut, in.back()); // constant continuation (no slope info)
        return out;
    }

    const double y0 = in[in.size() - 2];
    const double y1 = in[in.size() - 1];
    const double slope = y1 - y0; // assumes unit x-step between samples
    //DBG(slope);

    // Start from y1 + slope (true continuation; do not repeat y1)
    double y = y1;
    for (std::size_t i = 0; i < numOut; ++i) {
        y += slope;
        out.push_back(y);
    }

    return out;
}

double PluginTestowy2AudioProcessor::alphaStageFromImpulseDecayMs(float T_s, double fs)
{
    const double eps = 0.5; // %
    int K = (int)std::lround(T_s * fs);
    if (K < 1) K = 1;

    const double b = std::exp(std::log(eps / (K + 1.0)) / K); // b = 1 - alpha
    const double alpha = 1.0 - b;
    return juce::jlimit(0.0, 1.0, alpha);
}

double PluginTestowy2AudioProcessor::alphaFromStepResponseTimeEMA(float tau_s, double fs) {

    alpha = 1 - std::exp((-1 / fs) / tau_s);

    return juce::jlimit(0.0, 1.0, alpha);
}


void PluginTestowy2AudioProcessor::smoothRatios(std::vector<double>& ratios, double alpha)
{
    for (auto& r : ratios)
    {   
        ratioLPState += alpha * (r - ratioLPState);
        r = ratioLPState;
    }
}

void PluginTestowy2AudioProcessor::smoothRatiosTwoStage(std::vector<double>& ratios, double alpha)
{
    // alpha w [0,1]
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;


    //const double a = 1.0 - std::sqrt(1.0 - alpha);

    for (auto& r : ratios)
    {
        // Stopieñ 1
        ratioLPStateStage1_ += alpha * (r - ratioLPStateStage1_);

        ratioLPState += alpha * (ratioLPStateStage1_ - ratioLPState);

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
        juce::NormalisableRange<float>(-8.0f, 8.0f, 0.1f), 0.0f)
    );

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TauTouch",
        "Tau Touch",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.5f ), 0.04f)
    );
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TauFree",
        "Tau Free",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f, 1.0f), 0.5f)
    );

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "FilterOn",
        "Filter",
        false
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

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "TempoMode",
        "Tempo Mode",
        false
    ));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ScratchScale",
        "Scratch Scale",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.01), 1.0f)
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

ThreeChannelWavLogger logger;
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
    ratioLPStateStage1_ = 0.0;
    ratioLPState = 0.0;

    bufferID = 0;

    lut = ttvst::lutSinc::generateLutSinc(16384,2331, 0.45);

    juce::File out = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("test_one_pole_4.wav");

    auto r = logger.start(out, sampleRate, 24, { 0, 1 }); // map buffer ch0->file0, ch1->file1
    if (r.failed())
        DBG("logger start failed: " + r.getErrorMessage());
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
                fileSR = published ->sampleRate;
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
int PluginTestowy2AudioProcessor::getFileSR() const {
    return fileSR;
}

void PluginTestowy2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    using namespace ttvst::helps;
    using namespace ttvst::splines;
    using namespace ttvst::lutSinc;
    juce::ScopedNoDenormals _;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Capture MIDI for the UI
    for (const auto metadata : midiMessages)
        midiLog_.pushFromAudioThread(metadata.getMessage(), metadata.samplePosition, bufferID);

    bufferID++;
    const bool tempoMode = apvts.getRawParameterValue("TempoMode")->load();
    //capture touchdown state
    for (const auto meta : midiMessages)
    {
        const auto& m = meta.getMessage();
        if (m.isController() && m.getControllerNumber() == 64) {
            touchDown_ = (m.getControllerValue() >= 64);
            if (tempoMode) {
                if (touchDown_) {
                    playheadOnTouchdown_ = playhead_;
                }
                if (!touchDown_) {
                    playhead_ = playheadOnTouchdown_;
                }
            }

        }
        
    }




    buffer.clear();
    offsets_ = {};
    values_ = {};
    //ratios_ = {};
    speeds_ = {};
    speed_offsets_ = {};
    
    const bool motorOn = apvts.getRawParameterValue("motorOn")->load();
    const bool filterOn = apvts.getRawParameterValue("FilterOn")->load();
    const float pitchShift = apvts.getRawParameterValue("PitchShift")->load();
    const float tauTouch = apvts.getRawParameterValue("TauTouch")->load();
    const float tauFree = apvts.getRawParameterValue("TauFree")->load();
    baseCutoff = apvts.getRawParameterValue("FilterBaseCutoff")->load();
    filterAlpha = apvts.getRawParameterValue("FilterAlpha")->load();
    const float scratchScale = apvts.getRawParameterValue("ScratchScale")->load();
    const double motorSpeed = motorOn ? (1.0 + (1.0 * pitchShift / 8.0)) : 0.0;
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

    if (touchDown_) {
        playheadOnTouchdown_ += outN * motorSpeed;
    }

    //auto optOff = getPitchWheelOffsetsVector(midiMessages, 1);
    auto optVal = getPitchWheelValueVector(midiMessages, 1);
    if (optVal) {
        //auto ofs = optOff.value();
        //std::transform(ofs.begin(), ofs.end(), ofs.begin(), [outN](float val) { return val + outN; });
        //std::vector<double >values = pitchWheelToSamplePositionVec(*optVal, scratchScale);
        std::vector<double >values = optVal.value();
        afterRenderOffsetVec = { (double)2 * outN - 1 };
        afterRenderValueVec = values;
        pitchEmptyStreak_ = 0;
    }
    else {
        afterRenderOffsetVec = {};
        afterRenderValueVec = {};
        pitchEmptyStreak_++;
        //DBG("buffer empty");
        //count empty buffers
        
    }
    


    
    // creating vectors for messages in index range: < - outN, outN > -- moving in out memory buffers
    offsets_.insert(offsets_.end(), preRenderOffsetVec.begin(), preRenderOffsetVec.end());
    offsets_.insert(offsets_.end(), thisOffsetVec.begin(), thisOffsetVec.end());
    offsets_.insert(offsets_.end(), afterRenderOffsetVec.begin(), afterRenderOffsetVec.end());

    for (auto o : offsets_) {
        DBG(o);
    }


    values_.insert(values_.end(), preRenderValueVec.begin(), preRenderValueVec.end());
    values_.insert(values_.end(), thisValueVec.begin(), thisValueVec.end());
    values_.insert(values_.end(), afterRenderValueVec.begin(), afterRenderValueVec.end());


    ttvst::helps::vectorPairDbl speedinfo = positionsToSpeedWrapped(values_, offsets_, outN, 2);
    speeds_ = speedinfo.first;
    speed_offsets_ = speedinfo.second;
    
    speeds_ = pitchWheelToSamplePositionVec(speeds_, scratchScale);


    catchSpeedOutliers(speeds_, 6.0);

    // if the interpolation stream just starts, insert the last generated speed (form smoothed speed)
    if (!splineCondition_.has_value()) {
        insertBaseSpeed(speeds_, speed_offsets_, ratioLPState);
    }


    if (speeds_.size() > 1) {

        //alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_ * tauTouch));
        alpha = alphaStageFromImpulseDecayMs(tauTouch, hostSampleRate_);
        ratios_ = {};
        splineSetPlus splineSetPlus_ = splineSpecial(speed_offsets_, speeds_, splineCondition_, 1, outN);
        splineSet_ = splineSetPlus_.set;
        splineCondition_ = splineSetPlus_.spline_condition;

        if (lastSpline.x < 0) {
            splineSet_.insert(splineSet_.begin(), lastSpline);
        }

        lastSpline = splineSetPlus_.jointSpline;
        lastSpline.x = lastSpline.x - outN;

        ratios_ = createSpeedVector(splineSet_, outN);
        
    }
    else
    {
        if (pitchEmptyStreak_ <= 2)
        {
            //alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_*tauFree));
            alpha = alphaFromStepResponseTimeEMA(tauTouch, hostSampleRate_);
            ratios_ = linearContinuationFromLastSlope(ratios_, outN);
            lastSpline = {};
            splineSet_ = {};
            splineCondition_.reset();
        }
        else if (pitchEmptyStreak_ == 3 && touchDown_)
        {
            //alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_ * tauFree));
            alpha = alphaFromStepResponseTimeEMA(tauTouch, hostSampleRate_);
            ratios_.assign(outN, 0.0);

            splineSet_ = {};
            lastSpline = {};
            splineCondition_.reset();
        }
        else if (pitchEmptyStreak_ > 3 && touchDown_)
        {
            //alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_ * tauTouch));
            alpha = alphaFromStepResponseTimeEMA(tauTouch, hostSampleRate_);
            ratios_.assign(outN, 0.0);

            splineSet_ = {};
            lastSpline = {};
            splineCondition_.reset();
        }
        else
        {
            //alpha = 1.0 - std::exp(-1.0 / (hostSampleRate_ * tauFree));
            alpha = alphaFromStepResponseTimeEMA(tauFree, hostSampleRate_);
            ratios_.assign(outN, motorSpeed);

            splineSet_ = {};
            lastSpline = {};
            splineCondition_.reset();
        }
    }


    if (ratios_.size() == outN) {
        logger.pushFromAudioThread(buffer, ratios_);
        smoothRatios(ratios_, alpha);
        if (!ratios_.empty() && std::isfinite(ratios_.back())) {
            lastGoodSpeed_ = ratios_.back();
        }
        float cutofff;
        const float* lutPtr = lut.data();
        for (int i = 0; i < outN; i++){

            wrapPlayhead(playhead_, srcN);

            const float speedAbs = std::abs(ratios_[i]);
            const float cutoff = baseCutoff * std::pow(speedAbs, filterAlpha);
            const float cutoffClamped = juce::jlimit(50.0f, 0.45f * (float)hostSampleRate_, cutoff);

            for (int ch = 0; ch < outCh; ch++){
                
                //float out = interpolateHermiteCatmullRom(data->buffer, ch, playhead_, srcN);
                float out = interpolateSincLUT_PhaseLerp(data->buffer, ch, playhead_, srcN, lutPtr, 16384, 2331);
                //float out = interpolateLinear(data->buffer, ch, playhead_, srcN);
                if (filterOn){

                    out = (ch == 0) ? lpfLeft.processSample(out, cutoffClamped)
                                    : lpfRight.processSample(out, cutoffClamped);
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

    //append_vector_csv("test_csv.csv", ratios_, 16);

    //logger.pushFromAudioThread(buffer, ratios_);
    
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


    //if (hasPitchWheelMessage(midiMessages)) {
    //    lastMidi_.swapWith(midiMessages);
    //}
    //else {
    //    lastMidi_.clear();
    //}







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
