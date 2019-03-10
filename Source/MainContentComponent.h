#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <Windows.h>
#include "../ExternalLib/json.hpp"
#include "MonitorComponent.h"
#include "IOComponent.h"
#include "FilesComponent.h"
#include "BinaryData.h"

#define NUM_MIDI_CHANNELS 16

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

// GUI Constants
#define EXT_MARGIN 5
#define INT_MARGIN 3

using json = nlohmann::json;

//==============================================================================
class MainContentComponent : public Component,
	private MidiInputCallback, ActionListener
{
public:
	MainContentComponent()
	{
		setOpaque(true);

		// Set up device manager to pass to IO --> there should be a better way
		deviceManager = new AudioDeviceManager();

		// MIDI IO
		io.setDeviceManager(deviceManager);
		addAndMakeVisible(io);
		io.addListener(this);

		// MIDI Zones Management
		addAndMakeVisible(files);
		files.addListener(this);
		currentFileIdx = 0;
		for (uint8_t channel = 0; channel < NUM_MIDI_CHANNELS; ++channel) {
			currentZones[channel] = initializeZones();
		}

		// MIDI Display
		addAndMakeVisible(monitor);
		
		setSize(1000, 700);
	}

	~MainContentComponent()
	{
		delete deviceManager;
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colours::black);
	}

	void resized() override
	{
		io.setBounds(EXT_MARGIN, EXT_MARGIN, getWidth() / 2 - INT_MARGIN *2, getHeight() / 2 - INT_MARGIN *2);
		files.setBounds(getWidth() / 2 + INT_MARGIN, EXT_MARGIN, getWidth() / 2 - INT_MARGIN - EXT_MARGIN, getHeight() / 2 - INT_MARGIN * 2);
		monitor.setBounds(EXT_MARGIN, getHeight() / 2 + INT_MARGIN, getWidth() - EXT_MARGIN * 2, getHeight() / 2 - INT_MARGIN - EXT_MARGIN);
	}

private:
	void actionListenerCallback(const String& message) override
	{
		if (message[0] == 'A') {
			deviceManager->addMidiInputCallback(message.substring(1, message.length()), this);
		}
		else if (message[0] == 'D') {
			deviceManager->removeMidiInputCallback(message.substring(1, message.length()), this);
		}
		else if (message.compare("openDirectory") == 0) {
			setlist.clear();
			currentFileIdx = 0;
			programChangesList = files.getProgramChangesList();
			setlist = files.getSetlist();
			currentZones = setlist[currentFileIdx];
			sendProgramChanges();
		}
		else if (message.compare("loadCCMapping") == 0) {
			ccMapping.clear();
			ccMappingChannels.clear();
			ccMapping = files.getCCMapping();
			ccMappingChannels = files.getCCMappingChannels();
		}
		else if (message.compare("loadPreviousFile") == 0) {
			this->currentFileIdx = files.getCurrentFileIdx();
			this->currentZones = setlist[currentFileIdx];
			sendProgramChanges();
		}
		else if (message.compare("loadNextFile") == 0) {
			this->currentFileIdx = files.getCurrentFileIdx();
			this->currentZones = setlist[currentFileIdx];
			sendProgramChanges();
		}
	}

	void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override
	{
		MidiMessage newMessage(message.getRawData(), message.getRawDataSize(), message.getTimeStamp());
		if (message.isNoteOnOrOff()) {
			// Find the zone
			try {
				std::vector<int> zoneIndices = findZones(message.getChannel(), message.getNoteNumber());
				for (int index : zoneIndices) {
					// Change the channel
					newMessage.setChannel((int)currentZones[message.getChannel()][index]["outChannel"]);
					// Transpose
					newMessage.setNoteNumber(message.getNoteNumber() + (int)currentZones[message.getChannel()][index]["transpose"]);
					// Send message
					io.sendMIDIMessage(newMessage);
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
			if (programChangeNumber == 0) files.loadPreviousFile();
			if (programChangeNumber == 1) files.loadNextFile();
			if (programChangeNumber == 4 || programChangeNumber == 5 || programChangeNumber == 6) changeOrchestraArticulation(programChangeNumber);
			if (programChangeNumber == 7) toggleLeslieState();
		}
		else if (message.isController()) {
			// Convert the CC
			if (ccMapping.find(message.getControllerNumber()) != ccMapping.end()) {
				newMessage = MidiMessage::controllerEvent(ccMappingChannels[message.getControllerNumber()], ccMapping[message.getControllerNumber()], message.getControllerValue());
			}
			io.sendMIDIMessage(newMessage);
			postMessageToList(newMessage, source->getName());
		} else {
			// MIDI Thru
			io.sendMIDIMessage(newMessage);
			postMessageToList(newMessage, source->getName());
		}
		postMessageToList(message, source->getName());
	}
	
	void sendProgramChanges() {
		json currentPC = programChangesList[currentFileIdx];
		for (auto pc : currentPC) {
			MidiMessage newMessage = MidiMessage::programChange(pc["outChannel"], pc["programChangeNumber"]);
			io.sendMIDIMessage(newMessage);
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
		io.sendMIDIMessage(newMessage);
		newMessage = MidiMessage::noteOff(ORCHESTRA_LOW_CHANNEL, noteNumber);
		io.sendMIDIMessage(newMessage);
		newMessage = MidiMessage::noteOn(ORCHESTRA_MID_CHANNEL, noteNumber, (uint8)127);
		io.sendMIDIMessage(newMessage);
		newMessage = MidiMessage::noteOff(ORCHESTRA_MID_CHANNEL, noteNumber);
		io.sendMIDIMessage(newMessage);
		newMessage = MidiMessage::noteOn(ORCHESTRA_HIGH_CHANNEL, noteNumber, (uint8)127);
		io.sendMIDIMessage(newMessage);
		newMessage = MidiMessage::noteOff(ORCHESTRA_HIGH_CHANNEL, noteNumber);
		io.sendMIDIMessage(newMessage);
	}

	void toggleLeslieState() {
		auto newMessage = MidiMessage::controllerEvent(B3_CHANNEL, B3_LESLIE_CC, 127*leslieState);
		io.sendMIDIMessage(newMessage);
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

	std::vector<int> findZones(int inChannel, int noteNumber) {
		std::vector<int> ret;
		for (int zoneIdx = 0; zoneIdx < currentZones[inChannel].size(); ++zoneIdx) {
			if (((int)(currentZones[inChannel][zoneIdx]["startNote"]) <= noteNumber) && (noteNumber <= (int)(currentZones[inChannel][zoneIdx]["endNote"]))) {
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
		monitor.logMessage(midiMessageString);
	}

	//==============================================================================
	AudioDeviceManager* deviceManager;
	
	// MIDI IO
	IOComponent io;

	// MIDI Display
	MonitorComponent monitor;

	// Zone File Management
	FilesComponent files;
	
	// Zones Management
	std::map<uint8_t, json> currentZones;
	std::vector<std::map<uint8_t, json>> setlist;
	std::vector<String> setlistNames;
	std::vector<json> programChangesList;
	int currentFileIdx;
	
	// CC Management
	std::map<int, int> ccMapping;
	std::map<int, int> ccMappingChannels;
	
	// B3 Leslie Management
	bool leslieState = false;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent);
};