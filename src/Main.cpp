#include <juce_gui_extra/juce_gui_extra.h>
#include "ui/MainComponent.h"
#include "PluginHost.h"
#include "Theme.h"
#include <iostream>

namespace artilody
{

class ArtilodyApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Artilody Tracker"; }
    const juce::String getApplicationVersion() override { return ARTILODY_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName = "Artilody Tracker";
        opts.filenameSuffix = ".settings";
       #if JUCE_LINUX || JUCE_BSD
        opts.folderName = ".config/artilody-tracker";
       #else
        opts.folderName = "Artilody Tracker";
       #endif
        opts.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters (opts);

        lookAndFeel = std::make_unique<ArtilodyLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        pluginHost = std::make_unique<PluginHost> (properties);
        mainWindow = std::make_unique<MainWindow> (*this, properties);

        if (commandLine.startsWith ("--plugin-test"))
        {
            setApplicationReturnValue (runPluginTest (commandLine.fromFirstOccurrenceOf ("--plugin-test", false, false).trim().unquoted()) ? 0 : 1);
            quit();
            return;
        }

        if (commandLine.startsWith ("--save-jazz-demo"))
        {
            auto args = juce::StringArray::fromTokens (commandLine.fromFirstOccurrenceOf ("--save-jazz-demo", false, false), true);
            args.removeEmptyStrings();
            auto cwd = juce::File::getCurrentWorkingDirectory();
            const bool ok = args.size() > 0
                && mainWindow->saveJazzDemo (cwd.getChildFile (args[0].unquoted()), args.size() > 1 ? cwd.getChildFile (args[1].unquoted()) : juce::File());
            setApplicationReturnValue (ok ? 0 : 1);
            quit();
            return;
        }

        if (commandLine.startsWith ("--roundtrip-test"))
        {
            auto out = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.fromFirstOccurrenceOf ("--roundtrip-test", false, false).trim().unquoted());
            const bool ok = mainWindow->roundTripTest (out);
            std::cout << "roundtrip " << (ok ? "OK" : "FAILED") << std::endl;
            setApplicationReturnValue (ok ? 0 : 1);
            quit();
            return;
        }

        if (commandLine.startsWith ("--render-demo"))
        {
            auto out = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.fromFirstOccurrenceOf ("--render-demo", false, false).trim().unquoted());
            const bool ok = mainWindow->renderTo (out);
            setApplicationReturnValue (ok ? 0 : 1);
            quit();
            return;
        }

        auto file = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.unquoted().trim());
        if (commandLine.isNotEmpty() && file.existsAsFile())
            juce::MessageManager::callAsync ([this, file] { if (mainWindow) mainWindow->openFile (file); });
    }

    void shutdown() override
    {
        mainWindow.reset();
        pluginHost.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel.reset();
        properties.closeFiles();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow == nullptr) { quit(); return; }
        mainWindow->tryClose();
    }

    /** Developer diagnostics: scan default plug-in folders, instantiate and run every plug-in found. */
    bool runPluginTest (const juce::String& filter)
    {
        auto& fm = pluginHost->formatManager;
        bool allOk = true;
        for (auto* format : fm.getFormats())
        {
            auto files = format->searchPathsForPlugins (format->getDefaultLocationsToSearch(), true, false);
            for (auto& f : files)
            {
                if (filter.isNotEmpty() && ! f.containsIgnoreCase (filter) && ! format->getName().equalsIgnoreCase (filter))
                    continue;
                std::cout << "Testing " << f << std::endl;
                juce::OwnedArray<juce::PluginDescription> types;
                format->findAllTypesForFile (types, f);
                for (auto* d : types)
                {
                    juce::String err;
                    auto p = fm.createPluginInstance (*d, 44100.0, 512, err);
                    if (p == nullptr)
                    {
                        std::cout << format->getName() << "  FAIL  " << d->name << "  " << err << std::endl;
                        allOk = false;
                        continue;
                    }
                    p->enableAllBuses();
                    p->prepareToPlay (44100.0, 512);
                    const int ch = juce::jmax (2, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
                    juce::AudioBuffer<float> buf (ch, 512);
                    double energy = 0.0;
                    for (int b = 0; b < 40; ++b)
                    {
                        buf.clear();
                        for (int c = 0; c < p->getTotalNumInputChannels(); ++c)
                            for (int i = 0; i < 512; ++i)
                                buf.setSample (c, i, 0.25f * std::sin ((float) (b * 512 + i) * 0.05f));
                        juce::MidiBuffer midi;
                        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
                        if (b == 30) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                        p->processBlock (buf, midi);
                        for (int c = 0; c < p->getTotalNumOutputChannels(); ++c)
                            energy += buf.getRMSLevel (c, 0, 512);
                    }
                    bool editorOk = true;
                    if (p->hasEditor())
                    {
                        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
                        editorOk = ed != nullptr;
                        if (ed != nullptr)
                        {
                            juce::DocumentWindow w ("test", juce::Colours::black, 0);
                            w.setUsingNativeTitleBar (true);
                            w.setContentNonOwned (ed.get(), true);
                            w.setVisible (true);
                            const int loops = juce::jmax (20, juce::SystemStats::getEnvironmentVariable ("ARTILODY_EDITOR_MS", "0").getIntValue() / 20);
                            for (int i = 0; i < loops; ++i)
                                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                            w.clearContentComponent();
                        }
                    }
                    juce::MemoryBlock state;
                    p->getStateInformation (state);
                    p->releaseResources();
                    std::cout << format->getName() << "  OK    " << d->name << (d->isInstrument ? " [instrument]" : " [effect]")
                              << "  in=" << p->getTotalNumInputChannels() << " out=" << p->getTotalNumOutputChannels()
                              << "  params=" << p->getParameters().size() << "  state=" << (int) state.getSize()
                              << "  energy=" << energy << "  editor=" << (p->hasEditor() ? (editorOk ? "yes" : "FAILED") : "none") << std::endl;
                }
            }
        }
        return allOk;
    }

    void anotherInstanceStarted (const juce::String&) override {}

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (ArtilodyApplication& a, juce::ApplicationProperties& props)
            : DocumentWindow ("Artilody Tracker", Theme::background, DocumentWindow::allButtons), app (a)
        {
            setUsingNativeTitleBar (true);
            content = new MainComponent (props);
            content->onTitleChanged = [this] (const juce::String& t) { setName (t); };
            setContentOwned (content, true);
            setResizable (true, true);
            setResizeLimits (1100, 720, 10000, 10000);

            auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                          ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea
                          : juce::Rectangle<int> (0, 0, 1600, 960);
            centreWithSize (juce::jmin (1680, area.getWidth() - 40), juce::jmin (1000, area.getHeight() - 40));

            if (auto icon = Theme::logoMark(); icon.isValid())
                setIcon (icon);
            setVisible (true);
        }

        void openFile (const juce::File& f)
        {
            content->loadFile (f);
        }

        bool renderTo (const juce::File& f) { return content->renderTo (f); }
        bool roundTripTest (const juce::File& f) { return content->roundTripTest (f); }
        bool saveJazzDemo (const juce::File& p, const juce::File& w) { return content->saveJazzDemo (p, w); }

        void tryClose()
        {
            content->requestQuit ([] (bool ok) { if (ok) juce::JUCEApplication::getInstance()->quit(); });
        }

        void closeButtonPressed() override { tryClose(); }

    private:
        ArtilodyApplication& app;
        MainComponent* content = nullptr;
    };

private:
    juce::ApplicationProperties properties;
    std::unique_ptr<ArtilodyLookAndFeel> lookAndFeel;
    std::unique_ptr<PluginHost> pluginHost;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace artilody

START_JUCE_APPLICATION (artilody::ArtilodyApplication)
