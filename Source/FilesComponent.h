#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "../ExternalLib/json.hpp"

// GUI Constants
#define EXT_MARGIN 10
#define INT_MARGIN 4
#define BUTTON_HEIGHT 20
#define FONT_SIZE 35

using json = nlohmann::json;

class FilesComponent    : public Component, ActionBroadcaster
{
public:
    FilesComponent();
    ~FilesComponent();

    void paint (Graphics&) override;
    void resized() override;

	void addListener(ActionListener* listener);
	void removeListener(ActionListener* listener);
	void removeAllListeners();

	void loadPreviousFile();
	void loadNextFile();

	std::vector<json> getProgramChangesList();
	std::vector<json> getBankSelectList();
	std::vector<std::map<uint8_t, json>> getSetlist();
	std::map<int, int> getCCMapping();
	std::map<int, int> getCCMappingChannels();
	int getCurrentFileIdx();
private:
	void openDirectory();
	void addToSetlist(json fileContent);
	json readFile(const File & fileToRead);
	void openCCMappingFile();
	json readCCMappingFile(File & fileToRead);
	void loadCCMapping(json newMapping);
	void printOnCurrentFileTextEditor(const String& m);

	// Zone File Management
	TextButton directoryOpenButton;
	TextButton previousFileButton;
	TextButton nextFileButton;
	TextEditor currentFileNameTextEditor;

	std::map<uint8_t, json> localCurrentZones;
	std::vector<std::map<uint8_t, json>> localSetlist;
	std::vector<String> localSetlistNames;
	std::vector<json> localProgramChangesList;
	std::vector<json> localBankSelectList;
	int localCurrentFileIdx;

	// CC Management
	Label keyboardName;
	TextButton ccMappingFileOpenButton;
	std::map<int, int> localCCMapping;
	std::map<int, int> localCCMappingChannels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilesComponent)
};
