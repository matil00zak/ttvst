/*
  ==============================================================================

    helpers.cpp
    Created: 1 Nov 2025 1:58:05am
    Author:  matjo

  ==============================================================================
*/

#include "helpers.h"
#include <algorithm>
namespace ttvst::helps {

    


    std::optional<juce::MidiMessageMetadata> getLastPitchWheelMessage(const juce::MidiBuffer& buffer){

        std::optional<juce::MidiMessageMetadata> last = std::nullopt;
        for (auto metadata : buffer)
        {
            if (metadata.getMessage().isPitchWheel()) {

                last = metadata;
            }
        }
        return last;
    }

    std::optional<juce::MidiMessageMetadata> getFirstPitchWheelMessage(const juce::MidiBuffer& buffer) {

        for (auto metadata : buffer)
        {
            if (metadata.getMessage().isPitchWheel()) {

                return metadata;
            }
        }
        return std::nullopt;
    }

    std::optional<ttvst::helps::pairVector> getPitchWheelMsgPairVec(const juce::MidiBuffer& buffer) {
        pairVector messages;
        if (buffer.isEmpty()) {
            return std::nullopt;
            DBG("No messages in this buffer");
        }
        for (const auto metadata : buffer) {
            const auto& m = metadata.getMessage();
            if (!m.isPitchWheel()) continue;
            const int offset = metadata.samplePosition;
            const int val = m.getPitchWheelValue();
            intPair msg = { offset, val };
            messages.push_back(msg);
            DBG("helpers::pairVec: midi messages in this buffer: " << messages.size());
        }
        if (messages.empty()) {
            return std::nullopt;
            DBG("No pitch wheel messages in this buffer");
        }
        return messages;
        
    }

    std::optional<std::vector<double>> getPitchWheelValueVector(const juce::MidiBuffer& buffer, int maxMessages) {
        std::vector<double> values;
        if (buffer.isEmpty()) {
            return std::nullopt;
            //DBG("No messages in this buffer");
        }
        int count = 0;
        for (const auto metadata : buffer) {
            const auto& m = metadata.getMessage();
            if (!m.isPitchWheel()) continue;
            int value = m.getPitchWheelValue();
            values.push_back(value);
            count++;
            if (count == maxMessages)
                break;
            //DBG("values in this buffer: " << value);
        }
        //DBG("values in this buffer: " << values.size());
        if (values.empty()) {
            return std::nullopt;
            //DBG("No pitch wheel messages in this buffer");
        }
        return values;
    }

    std::optional<std::vector<double>> getPitchWheelOffsetsVector(const juce::MidiBuffer& buffer, int maxMessages) {
        std::vector<double> offsets;
        if (buffer.isEmpty()) {
            return std::nullopt;
            //DBG("No messages in this buffer");
        }
        int count = 0;
        for (const auto metadata : buffer) {
            const auto& m = metadata.getMessage();
            if (!m.isPitchWheel()) continue;
            //offsets.push_back(metadata.samplePosition);
            offsets.push_back(1023);
            count++;
            if (count == maxMessages)
                break;

        }
        //DBG("helpers::offsetsVector: messages in this buffer: " << count);
        if (offsets.empty()) {
            return std::nullopt;
            //DBG("No pitch wheel messages in this buffer");
        }
        return offsets;
    }
    

    // this function writes to preallocated vectors offsets and values
    // max count is the vectors reserved size
    // base offset is the local time at which the buffers start time is percieved in the process block 
    // (for the lookahead buffer this is outN)
    void extractPitchWheelData(const juce::MidiBuffer& buffer, int& count, double* offsets, double* values, int maxCount, double baseOffset) {
        for (const auto metadata : buffer) {

            if (!metadata.getMessage().isPitchWheel())
                continue;

            if (count >= maxCount)
                break;

            offsets[count] = baseOffset + metadata.samplePosition;
            values[count] = metadata.getMessage().getPitchWheelValue();
            DBG("assigned: " << values[count] << "at: " << values[count]);
            ++count;
        }
    }


    bool hasPitchWheelMessage(const juce::MidiBuffer& buffer) {
        for (auto meta : buffer) {
            if (meta.getMessage().isPitchWheel()) {
                return true;
            }        
        }
        return false;
    }

    std::vector<double> pitchWheelToSamplePositionVec(std::vector<double> values, float scale) {
        if (!values.empty()) {
            std::for_each(values.begin(), values.end(), [scale](double& n) {
                n = (n / 16383.0) * scale * 48000.0;
                });
            return values;
        }
        else {
            return {};
        }

    }

    double pitchWheelToSamplePosition(const double value) {
        return (value / 16383.0) * 2.0 * 48000.0;
    }

    //old version
    std::vector<double> createRatiosVector(std::vector<double> Y, std::optional<double> preRenderValue) {
        if (Y.size() < 2) {
            return {};
        }
        std::vector<double> ratios;
        ratios.reserve(Y.size());

        if (preRenderValue.has_value()) {
            ratios.push_back(Y[0] - *preRenderValue);
        }


        for (int i = 0; i + 1 < Y.size(); i++) {
            ratios.push_back(Y[i + 1] - Y[i]);
        }
        return ratios;


    }


    //creates pair of speeds and offsets from the full values and offsets pairs
    //the pairs are only for the messages int THIS buffer and one lookahead message
    //product of this function is supposed to be interpolated
    //for the interpolation to cover the whole buffer, spline set needs inserting the last spline saved 
    vectorPairDbl positionsToSpeed(std::vector<double> values, std::vector<double> offsets, int outN, int lookahead) {
        //this should only return a vector if input vectors have messages from two neighbouring buffers
        std::vector<double> speeds;
        std::vector<double> speed_offsets;
        int next = 0;
        //double last_offset = 0;
        if (offsets.size() > 1) {
            for (int i = 0; i < offsets.size() - 1; i++) {
                if (offsets[i + 1] >= 0 && next < lookahead) {
                    double delta_t = offsets[i + 1] - offsets[i];
                    double delta_pos = values[i + 1] - values[i];
                    double speed = delta_pos / delta_t;
                    speed_offsets.push_back(offsets[i + 1]);
                    speeds.push_back(speed);
                    if (offsets[i + 1] > outN) { next += 1; };
                }
            }
        }
        else {
            return { {}, {} };
        }
        return { speeds, speed_offsets };
    }


    static inline double wrappedDelta(double prev, double next, double wrap)
    {
        double d = next - prev;
        const double half = wrap * 0.5;
        if (d > half) d -= wrap;
        if (d < -half) d += wrap;
        return d;
        DBG("delta:%d", d);
    }

    vectorPairDbl positionsToSpeedWrapped(std::vector<double> values,
        std::vector<double> offsets,
        int outN,
        int lookahead)
    {
        std::vector<double> speeds;
        std::vector<double> speed_offsets;
        int nextCount = 0;

        // set this to your wrap size:
        // - for MIDI pitch bend 14-bit: 16384 (values 0..16383)
        constexpr double WRAP = 16384.0;

        if (offsets.size() > 1 && values.size() > 1) {
            const size_t n = std::min(values.size(), offsets.size());

            for (size_t i = 0; i + 1 < n; i++) {
                if (offsets[i + 1] >= 0 && nextCount < lookahead) {
                    double delta_t = offsets[i + 1] - offsets[i];
                    if (delta_t == 0.0) continue; // avoid inf/NaN

                    double delta_pos = wrappedDelta(values[i], values[i + 1], WRAP);
                    double speed = delta_pos / delta_t;

                    speed_offsets.push_back(offsets[i + 1]);
                    speeds.push_back(speed);

                    if (offsets[i + 1] > outN) { nextCount += 1; }
                }
            }
            return { speeds, speed_offsets };
        }

        return { {}, {} };
    }



    void catchSpeedOutliers(std::vector<double>& speeds, double maxSpeedAbs) {
        for (int i = 0; i < speeds.size(); i++) {
            if (speeds[i] > maxSpeedAbs) {
                //DBG("helpers::catchSpeedOutliers: outlier value = " << speeds[i]);
                speeds[i] = copysign(maxSpeedAbs, speeds[i]);
            }
        }
    }
    //only call on start of midi stream - when there is no prerender data / lastSpline 
    void insertBaseSpeed(std::vector<double>& speeds, std::vector<double>& offsets, double baseSpeed) {
        offsets.insert(offsets.begin(), 0.0);
        speeds.insert(speeds.begin(), baseSpeed);
        //DBG("helpers::insertBaseSpeed: inserted base speed = " << baseSpeed);
    }


    void insertLastSpeed(std::vector<double>& speeds, std::vector<double>& offsets, double& lastSpeed, double& lastOffset, int outN) {
        if (lastOffset >= outN) {
            offsets.push_back(lastOffset);
            speeds.push_back(lastSpeed);
        }
    }


    void wrapPlayhead(double& playhead, long srcLength) {
        playhead = std::fmod(playhead, (double)srcLength);
        if (playhead < 0.0) {
            playhead += srcLength;
        }
    }





}







