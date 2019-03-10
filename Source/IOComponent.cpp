#include "../JuceLibraryCode/JuceHeader.h"
#include "IOComponent.h"

IOComponent::IOComponent()
{
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

}

IOComponent::~IOComponent()
{
	for (auto button : midiInputButtons) {
		if (button->getToggleState()) removeMidiInput(button->getButtonText());
	}
}

void IOComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));   // clear the background
}

void IOComponent::resized()
{
	midiInputsLabel.setBounds(				EXT_MARGIN,		EXT_MARGIN,																		getWidth() - EXT_MARGIN * 2,		BUTTON_HEIGHT);
	for (unsigned idx = 0; idx < midiInputButtons.size(); ++idx) {
		midiInputButtons[idx]->setBounds(	EXT_MARGIN,		EXT_MARGIN + BUTTON_HEIGHT + INT_MARGIN + idx * (INT_MARGIN + BUTTON_HEIGHT),	getWidth() - EXT_MARGIN * 2,		BUTTON_HEIGHT);
	}
	midiOutputListLabel.setBounds(			EXT_MARGIN,		getHeight() - EXT_MARGIN - BUTTON_HEIGHT * 2 - INT_MARGIN,						getWidth() - EXT_MARGIN * 2,		BUTTON_HEIGHT);
	midiOutputList.setBounds(				EXT_MARGIN,		getHeight() - EXT_MARGIN - BUTTON_HEIGHT,										getWidth() - EXT_MARGIN * 2,		BUTTON_HEIGHT);
}

void IOComponent::updateToggleState(ToggleButton* button) {
	auto state = button->getToggleState();
	if (state) addMidiInput(button->getButtonText());
	else removeMidiInput(button->getButtonText());
}

void IOComponent::addMidiInput(String name) {
	if (!deviceManager->isMidiInputEnabled(name))
		deviceManager->setMidiInputEnabled(name, true);
	this->sendActionMessage("A" + name);
}

void IOComponent::removeMidiInput(String name) {
	this->sendActionMessage("D" + name);
}

bool IOComponent::setMidiOutput(int index) {
	midiOutputDevice.reset(MidiOutput::openDevice(index));
	midiOutputList.setSelectedId(index + 1, dontSendNotification);
	lastOutputIndex = index;
	return midiOutputDevice != nullptr;
}

void IOComponent::sendMIDIMessage(const MidiMessage& message) {
	midiOutputDevice->sendMessageNow(message);
}

void IOComponent::addListener(ActionListener * listener)
{
	this->addActionListener(listener);
}

void IOComponent::removeListener(ActionListener * listener)
{
	this->removeActionListener(listener);
}

void IOComponent::removeAllListeners()
{
	this->removeAllActionListeners();
}

void IOComponent::setDeviceManager(AudioDeviceManager * deviceManager)
{
	this->deviceManager = deviceManager;
}
