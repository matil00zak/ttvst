#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <vector>

namespace ttvst {

    // Compact copyable event for the UI thread (no juce::MidiMessage on the RT path)
    struct MidiEvent
    {
        int  sampleOffset = 0;      // offset within current audio block
        int  type = 0;      // 0=Other, 1=NoteOn, 2=NoteOff, 3=CC, 4=Pitch
        int  channel = 0;      // 1..16
        int  data1 = 0;      // note/cc number
        int  data2 = 0;      // velocity/cc value
        int  pitchValue = 8192;   // 0..16383, 8192 is center

        juce::String toString() const
        {
            switch (type)
            {
            case 1: return juce::String::formatted("NoteOn  ch:%d note:%d vel:%d @%d", channel, data1, data2, sampleOffset);
            case 2: return juce::String::formatted("NoteOff ch:%d note:%d vel:%d @%d", channel, data1, data2, sampleOffset);
            case 3: return juce::String::formatted("CC      ch:%d cc:%d val:%d @%d", channel, data1, data2, sampleOffset);
            case 4: {
                const double norm = (pitchValue - 8192) / 8192.0; // ~[-1, +1]
                return juce::String::formatted("Pitch   ch:%d val:%d (%.3f) @%d", channel, pitchValue, norm, sampleOffset);
            }
            default: return juce::String::formatted("Other   ch:%d @%d", channel, sampleOffset);
            }
        }
    };

    /**
     * Single-producer (audio thread), single-consumer (message thread) ring buffer.
     * Lock-free, drops on overflow (better than blocking the audio thread).
     */
    class MidiMessageManager
    {
    public:
        MidiMessageManager() = default;

        // Called from processBlock (audio thread)
        void pushFromAudioThread(const juce::MidiMessage& m, int sampleOffset) noexcept
        {
            MidiEvent e;
            e.sampleOffset = sampleOffset;
            e.channel = m.getChannel();

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
                e.pitchValue = m.getPitchWheelValue(); // 0..16383, 8192 center
            }
            else
            {
                e.type = 0;
            }

            // SPSC ring push
            auto w = write_.load(std::memory_order_relaxed);
            auto next = (w + 1) & mask;
            if (next == read_.load(std::memory_order_acquire))
            {
                // Buffer full: drop (never block the audio thread)
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            buffer_[w] = e;
            write_.store(next, std::memory_order_release);
        }

        // Called from UI thread (e.g., editor Timer)
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

        // Optional: how many events were dropped due to overflow (debug UI)
        size_t getAndResetDroppedCount() noexcept
        {
            return dropped_.exchange(0, std::memory_order_acq_rel);
        }

    private:
        static constexpr size_t capacity = 2048;         // power-of-two
        static constexpr size_t mask = capacity - 1;

        std::array<MidiEvent, capacity> buffer_{};
        std::atomic<size_t> write_{ 0 };
        std::atomic<size_t> read_{ 0 };
        std::atomic<size_t> dropped_{ 0 };
    };

} // namespace ttvst
