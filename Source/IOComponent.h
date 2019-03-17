#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

// GUI Constants
#define EXT_MARGIN 10
#define INT_MARGIN 4
#define BUTTON_HEIGHT 20

class IOComponent    : public Component, ActionBroadcaster
{
public:
    IOComponent();
    ~IOComponent();

    void paint (Graphics&) override;
    void resized() override;

	void updateToggleState(ToggleButton * button);

	void addMidiInput(String name);

	void removeMidiInput(String name);

	bool setMidiOutput(int index);

	void sendMIDIMessage(const MidiMessage& message);
	void sendMIDIClockBeat();

	void addListener(ActionListener* listener);
	void removeListener(ActionListener* listener);
	void removeAllListeners();
	
	void setDeviceManager(AudioDeviceManager* deviceManager);

private:
	// MIDI Input
	AudioDeviceManager* deviceManager;
	Label midiInputsLabel;
	StringArray midiInputsNames;
	OwnedArray<ToggleButton> midiInputButtons;

	// MIDI Output
	std::unique_ptr<MidiOutput> midiOutputDevice;
	ComboBox midiOutputList;
	Label midiOutputListLabel;
	int lastOutputIndex = 0;
	bool isAddingFromMidiOutput = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IOComponent)
};
