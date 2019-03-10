#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#define MAX_NUMBER_MESSAGES 1000

class MonitorComponent    : public Component
{
public:
    MonitorComponent();
    ~MonitorComponent();

    void paint (Graphics&) override;
    void resized() override;

	void logMessage(const String& m);

private:
	TextEditor midiMessagesBox;
	uint16_t numberDisplayedMessages = 0;
	double startTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorComponent)
};
