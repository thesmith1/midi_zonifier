#include "../JuceLibraryCode/JuceHeader.h"
#include "MainContentComponent.h"

#define MIN_HEIGHT 700
#define MIN_WIDTH 1000
#define MAX_HEIGHT 10000
#define MAX_WIDTH 10000

class Application    : public JUCEApplication
{
public:
    //==============================================================================
    Application() {}

    const String getApplicationName() override       { return "MIDI Zonifier"; }
    const String getApplicationVersion() override    { return "1.0"; }

    void initialise (const String&) override         { mainWindow.reset (new MainWindow ("MIDI Zonifier", new MainContentComponent(), *this)); }
    void shutdown() override                         { mainWindow = nullptr; }

private:
    class MainWindow    : public DocumentWindow
    {
    public:
        MainWindow (const String& name, Component* c, JUCEApplication& a)
            : DocumentWindow (name, Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons),
              app (a)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (c, true);

           #if JUCE_ANDROID || JUCE_IOS
            setFullScreen (true);
           #else
            setResizable (true, false);
            setResizeLimits (MIN_WIDTH, MIN_HEIGHT, MAX_WIDTH, MAX_HEIGHT);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }

    private:
        JUCEApplication& app;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (Application)
