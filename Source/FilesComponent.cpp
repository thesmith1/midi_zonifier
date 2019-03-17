#include "../JuceLibraryCode/JuceHeader.h"
#include "FilesComponent.h"

FilesComponent::FilesComponent()
{
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
	addAndMakeVisible(currentFileNameTextEditor);
	currentFileNameTextEditor.setReadOnly(true);
	currentFileNameTextEditor.setFont(Font(FONT_SIZE, 0));
	printOnCurrentFileTextEditor("No file loaded...");

	// CC Management
	addAndMakeVisible(ccMappingFileOpenButton);
	ccMappingFileOpenButton.setButtonText("Open a CC mapping file...");
	ccMappingFileOpenButton.onClick = [this] {openCCMappingFile(); };
	keyboardName.attachToComponent(&ccMappingFileOpenButton, true);
}

FilesComponent::~FilesComponent()
{
}

void FilesComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));   // clear the background
}

void FilesComponent::resized()
{
	directoryOpenButton.setBounds(			EXT_MARGIN,						EXT_MARGIN,											getWidth() - EXT_MARGIN * 2,					BUTTON_HEIGHT);
	previousFileButton.setBounds(			EXT_MARGIN,						BUTTON_HEIGHT + EXT_MARGIN + INT_MARGIN,			getWidth() / 2 - EXT_MARGIN - INT_MARGIN,		BUTTON_HEIGHT);
	nextFileButton.setBounds(				getWidth() / 2 + INT_MARGIN,	BUTTON_HEIGHT + EXT_MARGIN + INT_MARGIN,			getWidth() / 2 - EXT_MARGIN - INT_MARGIN,		BUTTON_HEIGHT);
	currentFileNameTextEditor.setBounds(	EXT_MARGIN,						BUTTON_HEIGHT * 2 + EXT_MARGIN + INT_MARGIN * 2,	getWidth() - EXT_MARGIN * 2,					getHeight() - BUTTON_HEIGHT * 3 - EXT_MARGIN * 2 - INT_MARGIN * 3);
	ccMappingFileOpenButton.setBounds(		EXT_MARGIN,						getHeight() - BUTTON_HEIGHT - EXT_MARGIN,			getWidth() - EXT_MARGIN * 2,					BUTTON_HEIGHT);
}

std::vector<json> FilesComponent::getProgramChangesList() {
	return this->localProgramChangesList;
}

std::vector<std::map<uint8_t, json>> FilesComponent::getSetlist() {
	return this->localSetlist;
}

std::map<int, int> FilesComponent::getCCMapping() {
	return this->localCCMapping;
}

std::map<int, int> FilesComponent::getCCMappingChannels() {
	return this->localCCMappingChannels;
}

int FilesComponent::getCurrentFileIdx() {
	return this->localCurrentFileIdx;
}

void FilesComponent::openDirectory() {
	FileChooser fileChooser("Select the folder containing your setlist...",
		File::getSpecialLocation(File::userDesktopDirectory));
	if (fileChooser.browseForDirectory()) {
		File folder = fileChooser.getResult();
		DirectoryIterator iter(folder, false, "*.json", File::findFiles);
		localSetlist.clear();
		localSetlistNames.clear();
		while (iter.next()) {
			json file_content = readFile(iter.getFile());
			addToSetlist(file_content["zones"]);
			localProgramChangesList.push_back(file_content["programChanges"]);
			localSetlistNames.push_back(iter.getFile().getFileNameWithoutExtension());
		}
		localCurrentFileIdx = 0;
		localCurrentZones = localSetlist[0];
		char s[50];
		sprintf(s, "[%d/%d] %s", localCurrentFileIdx + 1, localSetlistNames.size(), localSetlistNames[localCurrentFileIdx]);
		printOnCurrentFileTextEditor(s);
		this->sendActionMessage("openDirectory");
	}
}

void FilesComponent::addToSetlist(json fileContent) {
	std::map<uint8_t, json> newZones;
	for (auto input : fileContent) {
		int inChannel = (int)input["inChannel"];
		json zones = input["zones"];
		newZones[inChannel] = zones;
	}
	localSetlist.push_back(newZones);
}

json FilesComponent::readFile(const File & fileToRead) {
	if (!fileToRead.existsAsFile()) return nullptr;
	FileInputStream inputStream(fileToRead);
	if (!inputStream.openedOk()) return nullptr;
	return json::parse(inputStream.readEntireStreamAsString().toStdString());
}

void FilesComponent::openCCMappingFile() {
	FileChooser fileChooser("Select the file containing the CC mapping...",
		File::getSpecialLocation(File::userDesktopDirectory),
		"*.json");
	if (fileChooser.browseForFileToOpen()) {
		File ccMappingFile(fileChooser.getResult());
		json newMapping = readCCMappingFile(ccMappingFile);
		loadCCMapping(newMapping);
	}
}

json FilesComponent::readCCMappingFile(File & fileToRead) {
	if (!fileToRead.existsAsFile()) return nullptr;
	FileInputStream inputStream(fileToRead);
	if (!inputStream.openedOk()) return nullptr;
	return json::parse(inputStream.readEntireStreamAsString().toStdString());
}

void FilesComponent::loadCCMapping(json newMapping) {
	localCCMapping.clear();
	localCCMappingChannels.clear();
	keyboardName.setText(newMapping["keyboardName"].get<std::string>(), dontSendNotification);
	auto mapping = newMapping["ccMapping"];
	for (auto entry : mapping) {
		localCCMapping[entry["CConKey"]] = entry["CConVST"];
		localCCMappingChannels[entry["CConKey"]] = entry["outChannel"];
	}
	this->sendActionMessage("loadCCMapping");
}

void FilesComponent::loadPreviousFile() {
	if (localCurrentFileIdx <= 0) return;
	localCurrentFileIdx = localCurrentFileIdx - 1;
	localCurrentZones = localSetlist[localCurrentFileIdx];
	this->sendActionMessage("loadPreviousFile");
	char s[50];
	sprintf(s, "[%d/%d] %s", localCurrentFileIdx + 1, localSetlistNames.size(), localSetlistNames[localCurrentFileIdx]);
	printOnCurrentFileTextEditor(s);
}

void FilesComponent::loadNextFile() {
	if (localCurrentFileIdx >= localSetlist.size() - 1) return;
	localCurrentFileIdx = localCurrentFileIdx + 1;
	localCurrentZones = localSetlist[localCurrentFileIdx];
	this->sendActionMessage("loadNextFile");
	char s[50];
	sprintf(s, "[%d/%d] %s", localCurrentFileIdx + 1, localSetlistNames.size(), localSetlistNames[localCurrentFileIdx]);
	printOnCurrentFileTextEditor(s);
}

void FilesComponent::addListener(ActionListener * listener)
{
	this->addActionListener(listener);
}

void FilesComponent::removeListener(ActionListener * listener)
{
	this->removeActionListener(listener);
}

void FilesComponent::removeAllListeners()
{
	this->removeAllActionListeners();
}

void FilesComponent::printOnCurrentFileTextEditor(const String& m)
{
	currentFileNameTextEditor.clear();
	currentFileNameTextEditor.insertTextAtCaret(m);
}