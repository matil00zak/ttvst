

//nieuzywany w wersji docelowej logger WAV do debugowania
// 5 CHANNEL WAV LOGGER, rozszerzony, nazwa 3channel nieaktualna
/*
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <algorithm>

class ThreeChannelWavLogger : private juce::Thread
{
public:
    ThreeChannelWavLogger()
    : juce::Thread("ThreeChannelWavLogger"),
      fifo((int) blockCount)
    {
        // Pre-allocate blocks
        for (auto& b : blocks)
        {
            b.buffer.setSize(5, (int) maxBlockSize, false, false, true);
            b.numSamples = 0;
        }
    }

    ~ThreeChannelWavLogger() override
    {
        stop();
    }


    juce::Result start(const juce::File& fileToCreateOrOverwrite,
                       double sampleRate,
                       int bitsPerSample = 24,
                       std::array<int, 2> sourceStereoChannels = {0, 1})
    {
        stop();

        if (sampleRate <= 0.0)
            return juce::Result::fail("Invalid sampleRate.");

        outFile = fileToCreateOrOverwrite;
        channelMap = sourceStereoChannels;

        if (outFile.existsAsFile())
            outFile.deleteFile();

        auto stream = std::unique_ptr<juce::FileOutputStream>(outFile.createOutputStream());
        if (! stream || ! stream->openedOk())
            return juce::Result::fail("Could not open output stream.");

        juce::WavAudioFormat wav;
        //writer.reset(wav.createWriterFor(stream.get(), sampleRate, 3, bitsPerSample, {}, 0));
        writer.reset(wav.createWriterFor(stream.get(),
            sampleRate,
            5,
            32,      // 32-bit
            {}, 0));
        if (! writer)
            return juce::Result::fail("Could not create WAV writer.");

        // writer owns the stream now
        stream.release();

        shouldRun.store(true, std::memory_order_release);
        startThread(juce::Thread::Priority::low);
        //DBG("Logger file: " + outFile.getFullPathName());
        return juce::Result::ok();
    }

    // Call from message thread
    void stop()
    {
        shouldRun.store(false, std::memory_order_release);
        signalThreadShouldExit();
        stopThread(2000);

        // finalize header / flush
        writer.reset();

        // reset fifo
        fifo.reset();
        writeIndex.store(0, std::memory_order_release);
        readIndex.store(0, std::memory_order_release);
    }

    bool isRunning() const { return writer != nullptr && shouldRun.load(std::memory_order_acquire); }


    bool pushFromAudioThread(const juce::AudioBuffer<float>& stereoBuffer,
                             const std::vector<double>& thirdChannelVector,
                             const std::vector<double>& fourthChannelVector,
                             const std::vector<double>& fifthChannelVector)
    {
        if (! isRunning())
            return false;

        const int numSamples = stereoBuffer.getNumSamples();
        if (numSamples <= 0)
            return true;

        if (stereoBuffer.getNumChannels() <= juce::jmax(channelMap[0], channelMap[1]))
            return false;

        int offset = 0;
        while (offset < numSamples)
        {
            const int chunk = juce::jmin((int) maxBlockSize, numSamples - offset);
            if (! pushChunk(stereoBuffer, thirdChannelVector, fourthChannelVector, fifthChannelVector, offset, chunk))
                return false; // FIFO full; remaining dropped
            offset += chunk;
        }

        return true;
    }



private:
    struct Block
    {
        juce::AudioBuffer<float> buffer; // 3ch
        int numSamples = 0;
    };

    static constexpr size_t blockCount  = 64;    // queue depth
    static constexpr size_t maxBlockSize = 2048; // max chunk written per queued block (tune to your IO)

    bool pushChunk(const juce::AudioBuffer<float>& stereoBuffer,
                   const std::vector<double>& thirdChannelVector,
                   const std::vector<double>& fourthChannelVector,
                   const std::vector<double>& fifthChannelVector,
                   int srcOffset,
                   int chunkSamples)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(1, start1, size1, start2, size2);

        if (size1 == 0)
            return false; // FIFO full

        auto& b = blocks[(size_t) start1];
        b.numSamples = chunkSamples;

        // copy ch0/ch1 from selected source channels
        b.buffer.copyFrom(0, 0, stereoBuffer, channelMap[0], srcOffset, chunkSamples);
        b.buffer.copyFrom(1, 0, stereoBuffer, channelMap[1], srcOffset, chunkSamples);

        // ch2 from vector (aligned per-sample)
        const int vecN3 = (int) thirdChannelVector.size();
        const int vecN4 = (int)fourthChannelVector.size();
        const int vecN5 = (int)fifthChannelVector.size();
        float* ch2 = b.buffer.getWritePointer(2);
        float* ch3 = b.buffer.getWritePointer(3);
        float* ch4 = b.buffer.getWritePointer(4);

        for (int i = 0; i < chunkSamples; ++i)
        {
            const int idx = srcOffset + i;
            float v3 = 0.0f;
            float v4 = 0.0f;
            float v5 = 0.0f;

            if (idx < vecN3)
                v3 = (float) thirdChannelVector[(size_t) idx];

            if (idx < vecN4)
                v4 = (float)fourthChannelVector[(size_t)idx];

            if (idx < vecN5)
                v5 = (float)fifthChannelVector[(size_t)idx];

            ch2[i] = v3;
            ch3[i] = v4;
            ch4[i] = v5;
        }

        fifo.finishedWrite(1);
        return true;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            if (! shouldRun.load(std::memory_order_acquire) || writer == nullptr)
            {
                wait(5);
                continue;
            }

            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToRead(1, start1, size1, start2, size2);

            if (size1 == 0)
            {
                wait(2);
                continue;
            }

            auto& b = blocks[(size_t) start1];
            const int n = b.numSamples;

            // disk write happens here (background thread)
            const bool ok = writer->writeFromAudioSampleBuffer(b.buffer, 0, n);
            juce::ignoreUnused(ok); // you can add error handling/logging

            fifo.finishedRead(1);
        }
    }

    juce::File outFile;
    std::array<int, 2> channelMap {0, 1};

    std::unique_ptr<juce::AudioFormatWriter> writer;

    std::array<Block, blockCount> blocks;
    juce::AbstractFifo fifo;

    std::atomic<bool> shouldRun { false };
    std::atomic<int> writeIndex { 0 }, readIndex { 0 };
};
*/