#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <Windows.h>

#define DEBUG 1

//==============================================================================
class MainContentComponent : public Component,
	private MidiInputCallback,
	private MidiKeyboardStateListener
{
public:
	MainContentComponent()
		: startTime(Time::getMillisecondCounterHiRes() * 0.001)
	{
		setOpaque(true);

		// MIDI In
		addAndMakeVisible(midiInputListLabel);
		midiInputListLabel.setText("MIDI Input:", dontSendNotification);
		midiInputListLabel.attachToComponent(&midiInputList, true);

		addAndMakeVisible(midiInputList);
		midiInputList.setTextWhenNoChoicesAvailable("No MIDI Inputs Enabled");
		auto midiInputs = MidiInput::getDevices();
		midiInputList.addItemList(midiInputs, 1);
		midiInputList.onChange = [this] { setMidiInput(midiInputList.getSelectedItemIndex()); };

		// find the first enabled device and use that by default
		for (auto midiInput : midiInputs)
		{
			if (deviceManager.isMidiInputEnabled(midiInput))
			{
				setMidiInput(midiInputs.indexOf(midiInput));
				break;
			}
		}

		// if no enabled devices were found just use the first one in the list
		if (midiInputList.getSelectedId() == 0)
			setMidiInput(0);

		// MIDI Out
		addAndMakeVisible(midiOutputListLabel);
		midiOutputListLabel.setText("MIDI Output:", dontSendNotification);
		midiOutputListLabel.attachToComponent(&midiOutputList, true);

		addAndMakeVisible(midiOutputList);
		midiOutputList.setTextWhenNoChoicesAvailable("No MIDI Outputs Enabled");
		auto midiOutputs = MidiOutput::getDevices();
		midiOutputList.addItemList(midiOutputs, 1);
		midiOutputList.onChange = [this] { setMidiOutput(midiOutputList.getSelectedItemIndex()); };

		for (auto midiOutput : midiOutputs) {
			if (setMidiOutput(midiOutputs.indexOf(midiOutput)) != NULL) {
				break;
			}
		}

		// if no enabled devices were found just use the first one in the list
		if (midiOutputList.getSelectedId() == 0)
			setMidiOutput(0);

		// MIDI Display
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

		setSize(600, 400);
	}

	~MainContentComponent()
	{
		deviceManager.removeMidiInputCallback(MidiInput::getDevices()[midiInputList.getSelectedItemIndex()], this);
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colours::black);
	}

	void resized() override
	{
		auto area = getLocalBounds();

		midiInputList.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		midiOutputList.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		midiMessagesBox.setBounds(area.reduced(8));
	}

private:
	static String getMidiMessageDescription(const MidiMessage& m)
	{
		if (m.isNoteOn())           return "Note on " + MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3);
		if (m.isNoteOff())          return "Note off " + MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3);
		if (m.isProgramChange())    return "Program change " + String(m.getProgramChangeNumber());
		if (m.isPitchWheel())       return "Pitch wheel " + String(m.getPitchWheelValue());
		if (m.isAftertouch())       return "After touch " + MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3) + ": " + String(m.getAfterTouchValue());
		if (m.isChannelPressure())  return "Channel pressure " + String(m.getChannelPressureValue());
		if (m.isAllNotesOff())      return "All notes off";
		if (m.isAllSoundOff())      return "All sound off";
		if (m.isMetaEvent())        return "Meta event";

		if (m.isController())
		{
			String name(MidiMessage::getControllerName(m.getControllerNumber()));

			if (name.isEmpty())
				name = "[" + String(m.getControllerNumber()) + "]";

			return "Controller " + name + ": " + String(m.getControllerValue());
		}

		return String::toHexString(m.getRawData(), m.getRawDataSize());
	}

	void logMessage(const String& m)
	{
		midiMessagesBox.moveCaretToEnd();
		midiMessagesBox.insertTextAtCaret(m + newLine);
	}

	void setMidiInput(int index)
	{
		auto list = MidiInput::getDevices();

		deviceManager.removeMidiInputCallback(list[lastInputIndex], this);

		auto newInput = list[index];

		if (!deviceManager.isMidiInputEnabled(newInput))
			deviceManager.setMidiInputEnabled(newInput, true);

		deviceManager.addMidiInputCallback(newInput, this);
		midiInputList.setSelectedId(index + 1, dontSendNotification);

		lastInputIndex = index;
	}

	bool setMidiOutput(int index) {
		midiOutputDevice = MidiOutput::openDevice(index);
		midiOutputList.setSelectedId(index + 1, dontSendNotification);
		lastOutputIndex = index;
		return midiOutputDevice != nullptr;
	}

	void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override
	{
		const ScopedValueSetter<bool> scopedInputFlag(isAddingFromMidiInput, true);
		MidiMessage newMessage(message.getRawData(), message.getRawDataSize(), message.getTimeStamp());
		midiOutputDevice->sendMessageNow(newMessage);
		postMessageToList(message, source->getName());
	}

	void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
	{
		if (!isAddingFromMidiInput)
		{
			auto m = MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
			m.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
			postMessageToList(m, "On-Screen Keyboard");
		}
	}

	void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
	{
		if (!isAddingFromMidiInput)
		{
			auto m = MidiMessage::noteOff(midiChannel, midiNoteNumber);
			m.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
			postMessageToList(m, "On-Screen Keyboard");
		}
	}

	// This is used to dispach an incoming message to the message thread
	class IncomingMessageCallback : public CallbackMessage
	{
	public:
		IncomingMessageCallback(MainContentComponent* o, const MidiMessage& m, const String& s)
			: owner(o), message(m), source(s)
		{}

		void messageCallback() override
		{
			if (owner != nullptr)
				owner->addMessageToList(message, source);
		}

		Component::SafePointer<MainContentComponent> owner;
		MidiMessage message;
		String source;
	};

	void postMessageToList(const MidiMessage& message, const String& source)
	{
		(new IncomingMessageCallback(this, message, source))->post();
	}

	void addMessageToList(const MidiMessage& message, const String& source)
	{
		auto time = message.getTimeStamp() - startTime;

		auto hours = ((int)(time / 3600.0)) % 24;
		auto minutes = ((int)(time / 60.0)) % 60;
		auto seconds = ((int)time) % 60;
		auto millis = ((int)(time * 1000.0)) % 1000;

		auto timecode = String::formatted("%02d:%02d:%02d.%03d",
			hours,
			minutes,
			seconds,
			millis);

		auto description = getMidiMessageDescription(message);

		String midiMessageString(timecode + "  -  " + description + " (" + source + ")");
		logMessage(midiMessageString);
	}

	//==============================================================================
	// MIDI Input
	AudioDeviceManager deviceManager;
	ComboBox midiInputList;
	Label midiInputListLabel;
	int lastInputIndex = 0;
	bool isAddingFromMidiInput = false;

	// MIDI Output
	MidiOutput* midiOutputDevice;
	ComboBox midiOutputList;
	Label midiOutputListLabel;
	int lastOutputIndex = 0;
	bool isAddingFromMidiOutput = false;

	// MIDI Display
	TextEditor midiMessagesBox;
	double startTime;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent);
};