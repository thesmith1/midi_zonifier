#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <Windows.h>
#include "../ExternalLib/json.hpp"

#define DEBUG 0

#define MAX_NUMBER_MESSAGES 1000

#define MIN_NOTE_NUMBER 0
#define MAX_NOTE_NUMBER 127
#define DEFAULT_OUT_CHANNEL 1
#define ORCHESTRA_LOW_CHANNEL 1
#define ORCHESTRA_MID_CHANNEL 2
#define ORCHESTRA_HIGH_CHANNEL 3
#define ORCHESTRA_STACCATO_NOTE 12
#define ORCHESTRA_PIZZICATO_NOTE 13
#define ORCHESTRA_SUSTAIN_NOTE 14
#define B3_LESLIE_CC 82
#define B3_CHANNEL 4

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

		// CC Management
		addAndMakeVisible(ccMappingFileOpenButton);
		ccMappingFileOpenButton.setButtonText("Open a CC mapping file...");
		ccMappingFileOpenButton.onClick = [this] {openCCMappingFile(); };
		keyboardName.attachToComponent(&ccMappingFileOpenButton, true);

		setSize(1000, 700);
	}

	~MainContentComponent()
	{
		for (auto button : midiInputButtons) {
			if (button->getToggleState()) removeMidiInput(button->getButtonText());
		}
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
		ccMappingFileOpenButton.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		midiMessagesBox.setBounds(area.reduced(8));
	}

private:
	void logMessage(const String& m)
	{
		if (numberDisplayedMessages >= MAX_NUMBER_MESSAGES) {
			midiMessagesBox.clear();
			numberDisplayedMessages = 0;
		}
		numberDisplayedMessages++;
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
	
	bool setMidiOutput(int index) {
		midiOutputDevice.reset(MidiOutput::openDevice(index));
		midiOutputList.setSelectedId(index + 1, dontSendNotification);
		lastOutputIndex = index;
		return midiOutputDevice != nullptr;
	}

	void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override
	{
		MidiMessage newMessage(message.getRawData(), message.getRawDataSize(), message.getTimeStamp());
		if (message.isNoteOnOrOff()) {
			// Find the zone
			try {
				// int zoneIdx = findZone(message.getNoteNumber());
				std::vector<int> zoneIndices = findZones(message.getNoteNumber());
				for (int index : zoneIndices) {
					// Change the channel
					newMessage.setChannel((int)currentZones[index]["outChannel"]);
					// Transpose
					newMessage.setNoteNumber(message.getNoteNumber() + (int)currentZones[index]["transpose"]);
					// Send message
					midiOutputDevice->sendMessageNow(newMessage);
					postMessageToList(newMessage, source->getName());
				}
			}
			catch (const std::out_of_range) {
				// No output (no zone applied)
			}
			catch (nlohmann::detail::type_error) {
				// No output (no file loaded)
			}
		}
		else if (message.isProgramChange()) {
			const MessageManagerLock mmLock;
			// Change current file
			int programChangeNumber = message.getProgramChangeNumber();
			if (programChangeNumber == 0) loadPreviousFile();
			if (programChangeNumber == 1) loadNextFile();
			if (programChangeNumber == 4 || programChangeNumber == 5 || programChangeNumber == 6) changeOrchestraArticulation(programChangeNumber);
			if (programChangeNumber == 7) toggleLeslieState();
		}
		else if (message.isController()) {
			// Convert the CC
			if (ccMapping.find(message.getControllerNumber()) != ccMapping.end()) {
				newMessage = MidiMessage::controllerEvent(ccMappingChannels[message.getControllerNumber()], ccMapping[message.getControllerNumber()], message.getControllerValue());
			}
			midiOutputDevice->sendMessageNow(newMessage);
			postMessageToList(newMessage, source->getName());
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
				json file_content = readFile(iter.getFile());
				setlist.push_back(file_content["zones"]);
				programChangesList.push_back(file_content["programChanges"]);
				setlistNames.push_back(iter.getFile().getFileName());
			}
			currentFileIdx = 0;
			currentZones = setlist[currentFileIdx];
			sendProgramChanges();
			currentFileNameLabel.setText(setlistNames[currentFileIdx], dontSendNotification);
		}
	}

	json readFile(const File & fileToRead) {
		if (!fileToRead.existsAsFile()) return nullptr;
		FileInputStream inputStream(fileToRead);
		if (!inputStream.openedOk()) return nullptr;
		return json::parse(inputStream.readEntireStreamAsString().toStdString());
	}

	void openCCMappingFile() {
		FileChooser fileChooser("Select the file containing the CC mapping...",
			File::getSpecialLocation(File::userDesktopDirectory),
			"*.json");
		if (fileChooser.browseForFileToOpen()) {
			File ccMappingFile(fileChooser.getResult());
			json newMapping = readCCMappingFile(ccMappingFile);
			loadCCMapping(newMapping);
		}
	}

	json readCCMappingFile(File & fileToRead) {
		if (!fileToRead.existsAsFile()) return nullptr;
		FileInputStream inputStream(fileToRead);
		if (!inputStream.openedOk()) return nullptr;
		return json::parse(inputStream.readEntireStreamAsString().toStdString());
	}

	void loadCCMapping(json newMapping) {
		ccMapping.clear();
		ccMappingChannels.clear();
		keyboardName.setText(newMapping["keyboardName"].get<std::string>(), dontSendNotification);
		auto mapping = newMapping["ccMapping"];
		for (auto entry : mapping) {
			ccMapping[entry["CConKey"]] = entry["CConVST"];
			ccMappingChannels[entry["CConKey"]] = entry["outChannel"];
		}
	}

	void loadPreviousFile() {
		if (currentFileIdx <= 0) return;
		currentFileIdx = currentFileIdx - 1;
		currentZones = setlist[currentFileIdx];
		sendProgramChanges();
		currentFileNameLabel.setText(setlistNames[currentFileIdx], dontSendNotification);
	}

	void loadNextFile() {
		if (currentFileIdx >= setlist.size() - 1) return;
		currentFileIdx = currentFileIdx + 1;
		currentZones = setlist[currentFileIdx];
		sendProgramChanges();
		currentFileNameLabel.setText(setlistNames[currentFileIdx], dontSendNotification);
	}

	void sendProgramChanges() {
		json currentPC = programChangesList[currentFileIdx];
		for (auto pc : currentPC) {
			MidiMessage newMessage = MidiMessage::programChange(pc["outChannel"], pc["programChangeNumber"]);
			midiOutputDevice->sendMessageNow(newMessage);
		}
	}

	void changeOrchestraArticulation(int programChangeNumber) {
		int noteNumber;
		if (programChangeNumber == 4) {
			noteNumber = ORCHESTRA_SUSTAIN_NOTE;
		}
		else if (programChangeNumber == 5) {
			noteNumber = ORCHESTRA_STACCATO_NOTE;
		}
		else {
			noteNumber = ORCHESTRA_PIZZICATO_NOTE;
		}
		auto newMessage = MidiMessage::noteOn(ORCHESTRA_LOW_CHANNEL, noteNumber, (uint8)127);
		midiOutputDevice->sendMessageNow(newMessage);
		newMessage = MidiMessage::noteOff(ORCHESTRA_LOW_CHANNEL, noteNumber);
		midiOutputDevice->sendMessageNow(newMessage);
		newMessage = MidiMessage::noteOn(ORCHESTRA_MID_CHANNEL, noteNumber, (uint8)127);
		midiOutputDevice->sendMessageNow(newMessage);
		newMessage = MidiMessage::noteOff(ORCHESTRA_MID_CHANNEL, noteNumber);
		midiOutputDevice->sendMessageNow(newMessage);
		newMessage = MidiMessage::noteOn(ORCHESTRA_HIGH_CHANNEL, noteNumber, (uint8)127);
		midiOutputDevice->sendMessageNow(newMessage);
		newMessage = MidiMessage::noteOff(ORCHESTRA_HIGH_CHANNEL, noteNumber);
		midiOutputDevice->sendMessageNow(newMessage);
	}

	void toggleLeslieState() {
		auto newMessage = MidiMessage::controllerEvent(B3_CHANNEL, B3_LESLIE_CC, 127*leslieState);
		midiOutputDevice->sendMessageNow(newMessage);
		leslieState = !leslieState;
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

	std::vector<int> findZones(int noteNumber) {
		std::vector<int> ret;
		for (int zoneIdx = 0; zoneIdx < currentZones.size(); ++zoneIdx) {
			if (((int)(currentZones[zoneIdx]["startNote"]) <= noteNumber) && (noteNumber <= (int)(currentZones[zoneIdx]["endNote"]))) {
				ret.push_back(zoneIdx);
			}
		}
		return ret;
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
		String description;
		if (message.isNoteOnOrOff()) {
			std::stringstream ss;
			ss << "Note " << message.getNoteNumber() << " (" << message.getMidiNoteName(message.getNoteNumber(), true, true, 4) << ") Velocity " << (int)message.getVelocity() << " Channel " << message.getChannel();
			description = ss.str();
		}
		else if (message.isController()) {
			std::stringstream ss;
			ss << "CC" << message.getControllerNumber() << ": " << message.getControllerValue() << " Channel " << message.getChannel();
			description = ss.str();
		}
		else if (message.isProgramChange()) {
			std::stringstream ss;
			ss << "ProgramChange: " << message.getProgramChangeNumber() << " Channel " << message.getChannel();
			description = ss.str();
		}
		else {
			description = message.getDescription();
		}
		String midiMessageString(source + ": " + description);
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
	uint16_t numberDisplayedMessages = 0;
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
	std::vector<json> programChangesList;
	int currentFileIdx;

	// CC Management
	std::map<int, int> ccMapping;
	std::map<int, int> ccMappingChannels;
	Label keyboardName;
	TextButton ccMappingFileOpenButton;

	// B3 Leslie Management
	bool leslieState = false;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent);
};