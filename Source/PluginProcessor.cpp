/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>
#include <cstring>
#include <juce_audio_formats/codecs/flac/format.h>
#include <pluginterfaces/base/ftypes.h>
#include <wtypes.h>
#include <cmath>
#include "helpers.h"
#include "LutSincInterpolation.h"
//#include "WavLogger.cpp"



//==============================================================================
//nieuzywana struktura pomocnicza
struct Seg { int offset = 0; int value  = 0; };


// obliczanie wspolczynnika wygladzania ze stalej czasowej tau
double PluginTestowy2AudioProcessor::alphaFromStepResponseTimeEMA(float tau_s, double fs) {

    alpha = 1 - std::exp((-1 / fs) / tau_s);
    return juce::jlimit(0.0, 1.0, alpha);
}

// wygladzanie wektora r
void PluginTestowy2AudioProcessor::smoothRatios(std::vector<double>& ratios, double alpha)
{
    for (auto& r : ratios)
    {   
        ratioLPState += alpha * (r - ratioLPState);
        r = ratioLPState;
    }
}

// kaskada dwoch filtrow dla ratios, nieuzywane
void PluginTestowy2AudioProcessor::smoothRatiosTwoStage(std::vector<double>& ratios, double alpha)
{
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    for (auto& r : ratios)
    {
        ratioLPStateStage1_ += alpha * (r - ratioLPStateStage1_);
        ratioLPState += alpha * (ratioLPStateStage1_ - ratioLPState);
        r = ratioLPState;
    }
}

// wczytanie pliku zrodlowego do buffera
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
            (int)filePos,             // destStartSample
            toRead,                   // numSamples
            filePos,                  // start w pliku
            true, true))              // (left/right for stereo)
            break;

        filePos += toRead;
    }

    return out;
}

// odczyt wskaznika dla wczytanego pliku zrodlowego
LoadedAudioPtr PluginTestowy2AudioProcessor::getLoaded() const noexcept{
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
        "FilterAlpha",
        "Filter Alpha",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01), 1.0f)
    );

    
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
//obiekt loggera danych sterujacych
//ThreeChannelWavLogger logger;
//==============================================================================
void PluginTestowy2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1; // mono filter – u¿yjesz osobnego dla L i R

    hpLeft.prepare(spec);
    hpRight.prepare(spec);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
    hpLeft.coefficients = coeffs;
    hpRight.coefficients = coeffs;
    force_one_msg = false;
    check_offsets = false;
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
    //splineCondition_.reset();
    touch_vec = std::vector<double>(samplesPerBlock, 1.0);
    no_touch_vec = std::vector<double>(samplesPerBlock, 0.0);
    //lpfLeft.prepare(sampleRate);
    //lpfRight.prepare(sampleRate);
    //baseCutoff = 12000.0f;
    filterAlpha = 1.0;
    ratioLPStateStage1_ = 0.0;
    ratioLPState = 0.0;

    bufferID = 0;

    //lut = ttvst::lutSinc::generateLutSinc(512,27, 0.5);
    lut = ttvst::lutSinc::generateLutSinc(4096, 4095, 0.40);


    // Przygotowanie mechanizmu zapisu danych testowych
    //juce::File out = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
    //    .getChildFile("test_saw_48_410_4096_4095_04_f0_s_1_acc.wav");
    //juce::File out = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
    //    .getChildFile("predictibility_test.wav");
    //DBG("logger file: " + out.getFullPathName());
    //auto r = logger.start(out, sampleRate, 24, { 0, 1 }); // map buffer ch0->file0, ch1->file1
    //if (r.failed())
    //    DBG("logger start failed: " + r.getErrorMessage());
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
            fm.registerBasicFormats(); // WAV/AIFF/FLAC/MP3

            auto data = loadFileIntoAudioBuffer(fm, file); // std::shared_ptr<LoadedAudio>
            if (data)
            {   
                //DBG("Loaded: " << file.getFileName()
                //    << "  SR=" << data->sampleRate
                //    << "  ch=" << data->buffer.getNumChannels()
                //    << "  samples=" << data->buffer.getNumSamples());

                std::shared_ptr<const LoadedAudio> published = std::move(data);
                fileSR = published ->sampleRate;
                sampleRateRatio = fileSR / hostSampleRate_;
                std::atomic_store_explicit(&loaded_, published, std::memory_order_release);

                //DBG("LOADED");
            }
            else
            {
                //DBG("Failed to load: " << file.getFullPathName());
            }
        }).detach();
}

double PluginTestowy2AudioProcessor::getPlayheadSeconds() const {
    return playhead_ / fileSR;//getSampleRate();
}
int PluginTestowy2AudioProcessor::getFileSR() const {
    return fileSR;
}

void PluginTestowy2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    using namespace ttvst::helps;
    using namespace ttvst::lutSinc;
    juce::ScopedNoDenormals _;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    


    //CLEAR STUFF BEFORE PROCESSING
    buffer.clear();
    offsets_ = {};
    values_ = {};
    ratios_ = {};


    //LOAD BASIC PARAMETERS

    auto data = getLoaded();
    if (!data) return;

    const int srcCh = data->buffer.getNumChannels();
    const int srcN = data->buffer.getNumSamples();
    const int outCh = buffer.getNumChannels();
    const int outN = buffer.getNumSamples();
    if (srcN <= 0) return;


    if (lastBlock_.getNumChannels() != outCh || lastBlock_.getNumSamples() != outN) {
        lastBlock_.setSize(outCh, outN, false, true, true);
        haveLastMidi_ = false;
    }


    // aktualizacja parametrow GUI
    const bool motorOn = apvts.getRawParameterValue("motorOn")->load();
    const bool filterOn = apvts.getRawParameterValue("FilterOn")->load();
    const float pitchShift = apvts.getRawParameterValue("PitchShift")->load();
    const float tauTouch = apvts.getRawParameterValue("TauTouch")->load();
    const float tauFree = apvts.getRawParameterValue("TauFree")->load();
    const float scratchScale = apvts.getRawParameterValue("ScratchScale")->load() * fileSR * sampleRateRatio;
    const double motorSpeed = motorOn ? (sampleRateRatio+(sampleRateRatio * pitchShift / 100.0)) : 0.0;


    //aktualizacja stanu dotyku
    for (const auto meta : midiMessages)
    {
        const auto& m = meta.getMessage();
        if (m.isController() && m.getControllerNumber() == 64) {
            touchDown_ = (m.getControllerValue() >= 64);
        }
    }


    int pitchMsgsCount = appendPitchWheelMetadata(midiMessages,
        outN,
        afterRenderValueVec,
        afterRenderOffsetVec);
    DBG(pitchMsgsCount);
    
    //
    //repairPitchWheelMetadata(outN, afterRenderValueVec, afterRenderOffsetVec, force_one_msg);
    
    
    // licznik kolejnych buforow bez wiadomosci Pitch Bend
    if (pitchMsgsCount == 0) {
        if (pitchEmptyStreak_ < 5) {
            pitchEmptyStreak_++;
        }

    }
    else {
        pitchEmptyStreak_ = 0;
    }

    // laczenie zakresu trzech buforow na potrzeby prostszego przetwarzania 
    offsets_.insert(offsets_.end(), preRenderOffsetVec.begin(), preRenderOffsetVec.end());
    offsets_.insert(offsets_.end(), thisOffsetVec.begin(), thisOffsetVec.end());
    offsets_.insert(offsets_.end(), afterRenderOffsetVec.begin(), afterRenderOffsetVec.end());
    values_.insert(values_.end(), preRenderValueVec.begin(), preRenderValueVec.end());
    values_.insert(values_.end(), thisValueVec.begin(), thisValueVec.end());
    values_.insert(values_.end(), afterRenderValueVec.begin(), afterRenderValueVec.end());

    // strategia dzialania w przypadku kolejnych pustych buforow (Pitch Bend)
    if (touchDown_) {
        alpha = alphaFromStepResponseTimeEMA(tauTouch, hostSampleRate_);
        if (pitchEmptyStreak_ == 0) {
            appendNewPitchWheelSpeeds(speeds_, speed_offsets_, values_, offsets_, outN, scratchScale);
            lerpContinuityRestore(&speeds_, &speed_offsets_, outN);
        }
        if (pitchEmptyStreak_ == 1) {
            lerpContinuityContinue(&speeds_, &speed_offsets_, outN);
        }
        if (pitchEmptyStreak_ == 2) {
            speeds_.push_back(lastGoodSpeed_);
            speed_offsets_.push_back(2 * outN - 1);
            lerpContinuityRestore(&speeds_, &speed_offsets_, outN);
        }
        if (pitchEmptyStreak_ > 2) {
            speeds_.push_back(0.0);
            speed_offsets_.push_back(2 * outN - 1);
            lerpContinuityRestore(&speeds_, &speed_offsets_, outN);
        }
    }
    // brak dotyku
    else if (!touchDown_) {
        alpha = alphaFromStepResponseTimeEMA(tauFree, hostSampleRate_);
        speeds_.push_back(motorSpeed);
        speed_offsets_.push_back(2 * outN - 1);
        lerpContinuityRestore(&speeds_, &speed_offsets_, outN);
    }

    deleteOldPitchWheelSpeeds(speeds_, speed_offsets_, outN);
    
    // interpolacja liniowa pomiedzy wartosciami predkosci
    generateRatiosVectorLERP(&ratios_, &speeds_, &speed_offsets_, outN);

    // interpolacja audio dla wygenerowanych predkosci / pozycji odtwarzania. generacja bufora wyjsciowego na podstawie wektora r (ratios_)
    if (ratios_.size() == outN) {
        bufferID++;
        if (!ratios_.empty() && std::isfinite(ratios_.back())) {
            lastGoodSpeed_ = ratios_.back();
        }
        //ratios_before = ratios_;
        smoothRatios(ratios_, alpha);
        
        float cutofff;
        const float* lutPtr = lut.data();
        for (int i = 0; i < outN; i++){

            wrapPlayhead(playhead_, srcN);


            for (int ch = 0; ch < outCh; ch++){
                
                //float out = interpolateHermiteCatmullRom(data->buffer, ch, playhead_, srcN);
                float out = interpolateSincLUT_PhaseLerp(data->buffer, ch, playhead_, srcN, lutPtr, 4096, 4095);
                //float out = interpolateLinear(data->buffer, ch, playhead_, srcN);
                buffer.setSample(ch, i, out);
            
            }
            playhead_ += ratios_[i];
        }

        
    }

    // filtracja sygnalu wyjsciowego
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> leftContext(block.getSingleChannelBlock(0));
    juce::dsp::ProcessContextReplacing<float> rightContext(block.getSingleChannelBlock(1));
    hpLeft.process(leftContext);
    hpRight.process(rightContext);

    // dopisanie danych z bufora do wav. funkcjonalnosc przeznaczona do testow wtyczki
    //logger.pushFromAudioThread(buffer, ratios_, ratios_before, touchDown_ ? touch_vec : no_touch_vec);
    

    std::transform(afterRenderOffsetVec.begin(), afterRenderOffsetVec.end(), afterRenderOffsetVec.begin(),
        [outN](float val) { return val - outN; });
    
    std::transform(thisOffsetVec.begin(), thisOffsetVec.end(), thisOffsetVec.begin(),
        [outN](float val) { return val - outN; });

    std::transform(speed_offsets_.begin(), speed_offsets_.end(), speed_offsets_.begin(),
        [outN](float val) { return val - outN; });

    preRenderValueVec = thisValueVec;
    preRenderOffsetVec = thisOffsetVec;
    thisValueVec = afterRenderValueVec;
    thisOffsetVec = afterRenderOffsetVec;

    
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
