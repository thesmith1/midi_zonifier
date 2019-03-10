#include "../JuceLibraryCode/JuceHeader.h"
#include "MonitorComponent.h"

MonitorComponent::MonitorComponent() : startTime(Time::getMillisecondCounterHiRes() * 0.001)
{
	addAndMakeVisible(midiMessagesBox);
	midiMessagesBox.setMultiLine(true);
	midiMessagesBox.setReturnKeyStartsNewLine(true);
	midiMessagesBox.setReadOnly(true);
	midiMessagesBox.setScrollbarsShown(true);
	midiMessagesBox.setCaretVisible(false);
	midiMessagesBox.setPopupMenuEnabled(true);
	midiMessagesBox.setColour(TextEditor::backgroundColourId, Colour(0x32ffffff));
	midiMessagesBox.setColour(TextEditor::outlineColourId, Colour(0x1c000000));
	midiMessagesBox.setColour(TextEditor::shadowColourId, Colour(0x16000000));
}

MonitorComponent::~MonitorComponent()
{
}

void MonitorComponent::paint(Graphics& /*g*/)
{}

void MonitorComponent::resized()
{
    auto area = getLocalBounds();

	midiMessagesBox.setBounds(area);

}

void MonitorComponent::logMessage(const String& m)
{
	if (numberDisplayedMessages >= MAX_NUMBER_MESSAGES) {
		midiMessagesBox.clear();
		numberDisplayedMessages = 0;
	}
	numberDisplayedMessages++;
	midiMessagesBox.moveCaretToEnd();
	midiMessagesBox.insertTextAtCaret(m + newLine);
}