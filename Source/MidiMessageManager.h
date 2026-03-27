/*
#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <vector>

namespace ttvst {


    struct MidiEvent
    {
        int  sampleOffset = 0; 
        int  type = 0;
        int  channel = 0;
        int  data1 = 0;
        int  data2 = 0;
        int  pitchValue = 8192;
        int  bufferID = 666;





        juce::String toString() const
        {
            switch (type)
            {
            case 1: return juce::String::formatted("NoteOn  ch:%d note:%d vel:%d @%d", channel, data1, data2, sampleOffset);
            case 2: return juce::String::formatted("NoteOff ch:%d note:%d vel:%d @%d", channel, data1, data2, sampleOffset);
            case 3: return juce::String::formatted("CC      ch:%d cc:%d val:%d @%d", channel, data1, data2, sampleOffset);
            case 4: {
                const double norm = (pitchValue - 8192) / 8192.0; // ~[-1, +1]
                return juce::String::formatted("Pitch   ch:%d val:%d (%.3f) @%d bufferID:%d", channel, pitchValue, norm, sampleOffset, bufferID);
            }
            default:
                if (type == 0 && channel == 0)
                    return juce::String::formatted("Count   %d @%d", data1, sampleOffset);

                return juce::String::formatted("Other   ch:%d @%d", channel, sampleOffset);
            }
        }
    };


    class MidiMessageManager
    {
    public:
        MidiMessageManager() = default;

        void pushCountFromAudioThread(int count, int sampleOffset = 0) noexcept
        {
            MidiEvent e;
            e.sampleOffset = sampleOffset;
            e.type = 0;        // Other
            e.channel = 0;
            e.data1 = count;   // store the count here
            e.data2 = 0;

            auto w = write_.load(std::memory_order_relaxed);
            auto next = (w + 1) & mask;
            if (next == read_.load(std::memory_order_acquire))
            {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            buffer_[w] = e;
            write_.store(next, std::memory_order_release);
        }


        void pushFromAudioThread(const juce::MidiMessage& m, int sampleOffset, int bufferID) noexcept
        {
            MidiEvent e;
            e.sampleOffset = sampleOffset;
            e.channel = m.getChannel();
            e.bufferID = bufferID;

            if (m.isNoteOn()) { e.type = 1; e.data1 = m.getNoteNumber(); e.data2 = m.getVelocity(); }
            else if (m.isNoteOff()) { e.type = 2; e.data1 = m.getNoteNumber(); e.data2 = m.getVelocity(); }
            else if (m.isController())
            {
                e.type = 3;
                e.data1 = m.getControllerNumber();
                e.data2 = m.getControllerValue();
            }
            else if (m.isPitchWheel())
            {
                e.type = 4;
                e.pitchValue = m.getPitchWheelValue();
            }
            else
            {
                e.type = 0;
            }


            auto w = write_.load(std::memory_order_relaxed);
            auto next = (w + 1) & mask;
            if (next == read_.load(std::memory_order_acquire))
            {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            buffer_[w] = e;
            write_.store(next, std::memory_order_release);
        }

        void drainTo(std::vector<MidiEvent>& out) noexcept
        {
            auto r = read_.load(std::memory_order_relaxed);
            const auto w = write_.load(std::memory_order_acquire);

            while (r != w)
            {
                out.push_back(buffer_[r]);
                r = (r + 1) & mask;
            }
            read_.store(r, std::memory_order_release);
        }

        size_t getAndResetDroppedCount() noexcept
        {
            return dropped_.exchange(0, std::memory_order_acq_rel);
        }

    private:
        static constexpr size_t capacity = 2048;
        static constexpr size_t mask = capacity - 1;

        std::array<MidiEvent, capacity> buffer_{};
        std::atomic<size_t> write_{ 0 };
        std::atomic<size_t> read_{ 0 };
        std::atomic<size_t> dropped_{ 0 };
    };

} // namespace ttvst

*/
