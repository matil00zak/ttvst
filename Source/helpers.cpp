/*
  ==============================================================================

    helpers.cpp
    Created: 1 Nov 2025 1:58:05am
    Author:  matjo

  ==============================================================================
*/

#include "helpers.h"

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
            DBG("midi messages in this buffer: " << messages.size());
        }
        if (messages.empty()) {
            return std::nullopt;
            DBG("No pitch wheel messages in this buffer");
        }
        return messages;
        
    }

    std::optional<std::vector<int>> getPitchWheelValueVector(const juce::MidiBuffer& buffer) {
        std::vector<int> values;
        if (buffer.isEmpty()) {
            return std::nullopt;
            DBG("No messages in this buffer");
        }
        for (const auto metadata : buffer) {
            const auto& m = metadata.getMessage();
            if (!m.isPitchWheel()) continue;
            int value = m.getPitchWheelValue();
            values.push_back(value);
            DBG("values in this buffer: " << values.size());
        }
        if (values.empty()) {
            return std::nullopt;
            DBG("No pitch wheel messages in this buffer");
        }
        return values;
    }

    std::optional<std::vector<int>> getPitchWheelOffsetsVector(const juce::MidiBuffer& buffer) {
        std::vector<int> offsets;
        if (buffer.isEmpty()) {
            return std::nullopt;
            DBG("No messages in this buffer");
        }
        for (const auto metadata : buffer) {
            const auto& m = metadata.getMessage();
            if (!m.isPitchWheel()) continue;
            offsets.push_back(metadata.samplePosition);
            DBG("offsets in this buffer: " << offsets.size());
        }
        if (offsets.empty()) {
            return std::nullopt;
            DBG("No pitch wheel messages in this buffer");
        }
        return offsets;
    }
    

}







