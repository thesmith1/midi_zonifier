#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <Windows.h>
#include "../ExternalLib/json.hpp"

#define DEBUG 1

#define MIN_NOTE_NUMBER 0
#define MAX_NOTE_NUMBER 127
#define DEFAULT_OUT_CHANNEL 1

using json = nlohmann::json;

//==============================================================================
class MainContentComponent : public Component,
	private MidiInputCallback
{
public:
	MainContentComponent()
		: startTime(Time::getMillisecondCounterHiRes() * 0.001)
	{
		setOpaque(true);
		
		// MIDI In
		addAndMakeVisible(midiInputsLabel);
		midiInputsLabel.setText("Active MIDI Inputs:", dontSendNotification);
		midiInputsNames = MidiInput::getDevices();
		midiInputButtons = OwnedArray<ToggleButton>();
		for (auto in : midiInputsNames) {
			auto newButton = new ToggleButton(in);
			addAndMakeVisible(newButton);
			midiInputButtons.add(newButton);
			newButton->onClick = [this, newButton] { updateToggleState(newButton); };
		}
		
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

		// MIDI Zones Management
		addAndMakeVisible(directoryOpenButton);
		directoryOpenButton.setButtonText("Open a directory...");
		directoryOpenButton.onClick = [this] {openDirectory(); };
		addAndMakeVisible(previousFileButton);
		previousFileButton.setButtonText("Previous file");
		previousFileButton.onClick = [this] {loadPreviousFile(); };
		addAndMakeVisible(nextFileButton);
		nextFileButton.setButtonText("Next file");
		nextFileButton.onClick = [this] {loadNextFile(); };
		addAndMakeVisible(currentFileNameLabel);
		currentFileNameLabel.setText("No file loaded...", dontSendNotification);

		currentFileIdx = 0;
		currentZones = initializeZones();

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

		setSize(1000, 700);
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

		midiInputsLabel.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		for (auto button : midiInputButtons) {
			button->setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		}
		midiOutputList.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		directoryOpenButton.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		previousFileButton.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		nextFileButton.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		currentFileNameLabel.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
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

	void updateToggleState(ToggleButton* button) {
		auto state = button->getToggleState();
		if (state) addMidiInput(button->getButtonText());
		else removeMidiInput(button->getButtonText());
	}

	void addMidiInput(String name) {
		if (!deviceManager.isMidiInputEnabled(name))
			deviceManager.setMidiInputEnabled(name, true);
		deviceManager.addMidiInputCallback(name, this);
	}

	void removeMidiInput(String name) {
		deviceManager.removeMidiInputCallback(name, this);
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
		midiOutputDevice.reset(MidiOutput::openDevice(index));
		midiOutputList.setSelectedId(index + 1, dontSendNotification);
		lastOutputIndex = index;
		return midiOutputDevice != nullptr;
	}

	void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override
	{
		const ScopedValueSetter<bool> scopedInputFlag(isAddingFromMidiInput, true);
		MidiMessage newMessage(message.getRawData(), message.getRawDataSize(), message.getTimeStamp());
		if (message.isNoteOnOrOff()) {
			// Find the zone
			try {
				int zoneIdx = findZone(message.getNoteNumber());
				// Change the channel
				newMessage.setChannel((int)currentZones[zoneIdx]["outChannel"]);
				// Transpose
				newMessage.setNoteNumber(message.getNoteNumber() + (int)currentZones[zoneIdx]["transpose"]);
			}
			catch (const std::out_of_range) {
				// The message stays the same (no zone applied)
			}
			catch (nlohmann::detail::type_error) {
				// The message stays the same (no file loaded)
			}
			midiOutputDevice->sendMessageNow(newMessage);
			postMessageToList(newMessage, source->getName());
		}
		else if (message.isProgramChange()) {
			const MessageManagerLock mmLock;
			// Change current file
			int programChangeNumber = message.getProgramChangeNumber();
			if (programChangeNumber == 0) loadPreviousFile();
			if (programChangeNumber == 1) loadNextFile();
		} else {
			// MIDI Thru
			midiOutputDevice->sendMessageNow(newMessage);
			postMessageToList(newMessage, source->getName());
		}
		postMessageToList(message, source->getName());
	}
	
	void openDirectory() {
		FileChooser fileChooser("Select the folder containing your setlist...",
			File::getSpecialLocation(File::userDesktopDirectory));
		if (fileChooser.browseForDirectory()) {
			File folder = fileChooser.getResult();
			DirectoryIterator iter(folder, false, "*.json", File::findFiles);
			setlist.clear();
			while (iter.next()) {
				setlist.push_back(readFile(iter.getFile()));
				setlistNames.push_back(iter.getFile().getFileName());
			}
			currentFileIdx = 0;
			currentZones = setlist[currentFileIdx];
			currentFileNameLabel.setText(setlistNames[currentFileIdx], dontSendNotification);
		}
	}

	json readFile(const File & fileToRead) {
		if (!fileToRead.existsAsFile()) return nullptr;
		FileInputStream inputStream(fileToRead);
		if (!inputStream.openedOk()) return nullptr;
		return json::parse(inputStream.readEntireStreamAsString().toStdString())["zones"];
	}

	void loadPreviousFile() {
		if (currentFileIdx <= 0) return;
		currentFileIdx = currentFileIdx - 1;
		currentZones = setlist[currentFileIdx];
		currentFileNameLabel.setText(setlistNames[currentFileIdx], dontSendNotification);
	}

	void loadNextFile() {
		if (currentFileIdx >= setlist.size() - 1) return;
		currentFileIdx = currentFileIdx + 1;
		currentZones = setlist[currentFileIdx];
		currentFileNameLabel.setText(setlistNames[currentFileIdx], dontSendNotification);
	}

	json initializeZones() {
		json ret = {
			{"startNote", MIN_NOTE_NUMBER},
			{"endNote", MAX_NOTE_NUMBER},
			{"outChannel", DEFAULT_OUT_CHANNEL},
			{"transpose", 0}
		};
		return ret;
	}

	int findZone(int noteNumber) {
		for (int zoneIdx = 0; zoneIdx < currentZones.size(); ++zoneIdx) {
			if (((int)(currentZones[zoneIdx]["startNote"]) <= noteNumber) && (noteNumber <= (int)(currentZones[zoneIdx]["endNote"])))
				return zoneIdx;
		}
		throw new std::out_of_range("This note was outside any zone\n");
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

		String midiMessageString(timecode + "  -  " + message.getDescription() + " (" + source + ")");
		logMessage(midiMessageString);
	}

	//==============================================================================
	// MIDI Input
	AudioDeviceManager deviceManager;
	Label midiInputsLabel;
	StringArray midiInputsNames;
	OwnedArray<ToggleButton> midiInputButtons;

	// MIDI Output
	std::unique_ptr<MidiOutput> midiOutputDevice;
	ComboBox midiOutputList;
	Label midiOutputListLabel;
	int lastOutputIndex = 0;
	bool isAddingFromMidiOutput = false;

	// MIDI Display
	TextEditor midiMessagesBox;
	double startTime;

	// Zone File Management
	TextButton directoryOpenButton;
	TextButton previousFileButton;
	TextButton nextFileButton;
	Label currentFileNameLabel;

	// Zones Management
	json currentZones;
	std::vector<json> setlist;
	std::vector<String> setlistNames;
	int currentFileIdx;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent);
};