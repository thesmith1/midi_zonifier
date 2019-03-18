#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include <Windows.h>
#include "../ExternalLib/json.hpp"
#include "MonitorComponent.h"
#include "IOComponent.h"
#include "FilesComponent.h"
#include "BinaryData.h"
#include "aubio/aubio.h"

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
class MainContentComponent : public AudioAppComponent,
	private MidiInputCallback, ActionListener
{
public:
	MainContentComponent() : audioSetup(deviceManager,
		0, 256, 0, 256,
		false, false, false, false)
	{
		setOpaque(true);

		addAndMakeVisible(audioSetup);
		
		setAudioChannels(2, 0);

		// Set up device manager to pass to IO --> there should be a better way
		devices = new AudioDeviceManager();

		// MIDI IO
		io.setDeviceManager(devices);
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

		// Clock
		addAndMakeVisible(clockActiveButton);
		clockActiveButton.setButtonText("Enable Clock");
		clockActiveButton.onClick = [this] {
			if (clockActiveButton.getToggleState()) {
				clockActiveButton.setButtonText("Disable Clock");
			}
			else {
				clockActiveButton.setButtonText("Enable Clock");
			}
		};
		
		setSize(1000, 700);
	}

	~MainContentComponent()
	{
		del_aubio_tempo(beatTracker);
		shutdownAudio();
		delete devices;
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colours::black);
	}

	void resized() override
	{
		io.setBounds(					EXT_MARGIN,						EXT_MARGIN,								getWidth() / 2 - INT_MARGIN *2,				getHeight() / 2 - INT_MARGIN *2);
		files.setBounds(				getWidth() / 2 + INT_MARGIN,	EXT_MARGIN,								getWidth() / 2 - INT_MARGIN - EXT_MARGIN,	getHeight() / 2 - INT_MARGIN * 2);
		monitor.setBounds(				EXT_MARGIN,						getHeight() / 2 + INT_MARGIN,			getWidth() / 2 - INT_MARGIN * 2,			getHeight() / 2 - INT_MARGIN - EXT_MARGIN);
		audioSetup.setBounds(			getWidth() / 2 + INT_MARGIN,	getHeight() / 2 + INT_MARGIN,			getWidth() / 2 - INT_MARGIN - EXT_MARGIN,	getHeight() / 2 - INT_MARGIN*2 - EXT_MARGIN*2 - 20);
		clockActiveButton.setBounds(	getWidth() / 2 + INT_MARGIN,	getHeight() - EXT_MARGIN - INT_MARGIN - 20,	getWidth() / 2 - INT_MARGIN - EXT_MARGIN,	20);
	}

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
		inputAubioBuffer = new_fvec(samplesPerBlockExpected);
		beatTrackingResult = new_fvec(1);
		beatTracker = new_aubio_tempo("default", (u_int)(samplesPerBlockExpected * 2), (u_int)samplesPerBlockExpected, (u_int)sampleRate);
	}

	void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override {
		if (clockActiveButton.getToggleState()) {
			// Transfer JUCE audio buffer into Aubio buffer
			for (int sampleIdx = 0; sampleIdx < bufferToFill.numSamples; ++sampleIdx) {
				float sample = 0.0f;
				// Downmix to mono
				for (int chIdx = 0; chIdx < bufferToFill.buffer->getNumChannels(); ++chIdx) {
					sample += bufferToFill.buffer->getSample(chIdx, sampleIdx);
				}
				sample /= bufferToFill.buffer->getNumChannels();
				fvec_set_sample(inputAubioBuffer, (double)sample, sampleIdx);
			}
			bufferToFill.clearActiveBufferRegion();
			// Execute beat detection
			aubio_tempo_do(beatTracker, inputAubioBuffer, beatTrackingResult);
			// If beat
			if (beatTrackingResult->data[0] != 0) {
				io.sendMIDIClockBeat();
			}
		}
	}

	void releaseResources() override {}

private:
	void actionListenerCallback(const String& message) override
	{
		if (message[0] == 'A') {
			devices->addMidiInputCallback(message.substring(1, message.length()), this);
		}
		else if (message[0] == 'D') {
			devices->removeMidiInputCallback(message.substring(1, message.length()), this);
		}
		else if (message.compare("openDirectory") == 0) {
			setlist.clear();
			currentFileIdx = 0;
			programChangesList = files.getProgramChangesList();
			bankSelectList = files.getBankSelectList();
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
			io.sendNoteOffToAll();
			this->currentFileIdx = files.getCurrentFileIdx();
			this->currentZones = setlist[currentFileIdx];
			sendProgramChanges();
		}
		else if (message.compare("loadNextFile") == 0) {
			io.sendNoteOffToAll();
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
					json harmony = currentZones[message.getChannel()][index]["harmony"];
					if (!harmony.is_null()) { // If harmony exists
						bool isFound = false;
						for (json harmonyEl : harmony) {
							if (harmonyEl["inNote"] == message.getNoteNumber()) {
								isFound = true;
								bool canDo = true;
								if (message.isNoteOn() && !isHarmonyNoteOn) isHarmonyNoteOn = true;
								else if (message.isNoteOff() && !isHarmonyTwoNoteOn) isHarmonyNoteOn = false;
								else if (message.isNoteOn() && isHarmonyNoteOn) {
									isHarmonyTwoNoteOn = true;
									auto newMessage = MidiMessage::allNotesOff(message.getChannel());
									io.sendMIDIMessage(newMessage);
								}
								else if (message.isNoteOff() && isHarmonyTwoNoteOn) canDo = false;
								// Send messages
								if (canDo) {
									for (auto outNote : harmonyEl["outNotes"]) {
										newMessage.setChannel((int)currentZones[message.getChannel()][index]["outChannel"]);
										newMessage.setNoteNumber(outNote + (int)currentZones[message.getChannel()][index]["transpose"]);
										io.sendMIDIMessage(newMessage);
										postMessageToList(newMessage, source->getName());
									}
								}
							}
						}
						if (!isFound) {
							// Change the channel
							newMessage.setChannel((int)currentZones[message.getChannel()][index]["outChannel"]);
							// Transpose
							newMessage.setNoteNumber(message.getNoteNumber() + (int)currentZones[message.getChannel()][index]["transpose"]);
							// Send message
							io.sendMIDIMessage(newMessage);
							postMessageToList(newMessage, source->getName());
						}
					}
					else {
						// Change the channel
						newMessage.setChannel((int)currentZones[message.getChannel()][index]["outChannel"]);
						// Transpose
						newMessage.setNoteNumber(message.getNoteNumber() + (int)currentZones[message.getChannel()][index]["transpose"]);
						// Send message
						io.sendMIDIMessage(newMessage);
						postMessageToList(newMessage, source->getName());
					}
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
	}
	
	void sendProgramChanges() {
		json currentBS = bankSelectList[currentFileIdx];
		for (auto bs : currentBS) {
			MidiMessage newMessage = MidiMessage::controllerEvent(bs["outChannel"], 0, bs["bankNumber"]);
			io.sendMIDIMessage(newMessage);
		}
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
	AudioDeviceManager* devices;
	
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
	std::vector<json> bankSelectList;
	int currentFileIdx;
	
	// CC Management
	std::map<int, int> ccMapping;
	std::map<int, int> ccMappingChannels;
	
	// B3 Leslie Management
	bool leslieState = false;
	
	// Harmony NoteOnOff
	bool isHarmonyNoteOn = false;
	bool isHarmonyTwoNoteOn = false;

	// Clock
		// Audio In
	AudioDeviceSelectorComponent audioSetup;
		// Beat Tracking
	aubio_tempo_t* beatTracker;
	fvec_t* inputAubioBuffer;
	fvec_t* beatTrackingResult;
	ToggleButton clockActiveButton;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent);
};