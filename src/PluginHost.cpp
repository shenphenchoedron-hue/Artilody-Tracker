#include "PluginHost.h"
#include "ClapHost.h"
#include "Theme.h"

namespace artilody
{

static PluginHost* hostInstance = nullptr;

PluginHost& PluginHost::get()
{
    jassert (hostInstance != nullptr);
    return *hostInstance;
}

//==============================================================================
class PluginHost::PluginWindow final : public juce::DocumentWindow
{
public:
    PluginWindow (PluginHost& h, juce::AudioProcessor& p, const juce::String& title)
        : DocumentWindow (title, juce::Colour (0xff1b2433), DocumentWindow::closeButton | DocumentWindow::minimiseButton),
          host (h), processor (p)
    {
        setUsingNativeTitleBar (true);

        juce::AudioProcessorEditor* ed = nullptr;
        if (p.hasEditor())
            ed = p.createEditorIfNeeded();
        if (ed == nullptr)
            ed = new juce::GenericAudioProcessorEditor (p);

        if (auto* ge = dynamic_cast<juce::GenericAudioProcessorEditor*> (ed))
            ge->setSize (juce::jmax (420, ge->getWidth()), juce::jmin (600, juce::jmax (200, ge->getHeight())));

        setContentOwned (ed, true);
        setResizable (ed->isResizable() || dynamic_cast<juce::GenericAudioProcessorEditor*> (ed) != nullptr, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        toFront (true);
    }

    ~PluginWindow() override
    {
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        auto* p = &processor;
        auto* h = &host;
        juce::MessageManager::callAsync ([h, p] { h->closeEditor (p); });
    }

private:
    PluginHost& host;
    juce::AudioProcessor& processor;
};

//==============================================================================
PluginHost::PluginHost (juce::ApplicationProperties& props) : properties (props)
{
    hostInstance = this;
    formatManager.addDefaultFormats();
    formatManager.addFormat (new ClapPluginFormat());

    if (auto* settings = properties.getUserSettings())
        if (auto xml = settings->getXmlValue ("knownPlugins"))
            knownPlugins.recreateFromXml (*xml);

    knownPlugins.addChangeListener (this);
}

PluginHost::~PluginHost()
{
    knownPlugins.removeChangeListener (this);
    closeAllEditors();
    managerWindow.reset();
    if (hostInstance == this)
        hostInstance = nullptr;
}

void PluginHost::changeListenerCallback (juce::ChangeBroadcaster*)
{
    saveList();
}

void PluginHost::saveList()
{
    if (auto* settings = properties.getUserSettings())
        if (auto xml = knownPlugins.createXml())
        {
            settings->setValue ("knownPlugins", xml.get());
            settings->saveIfNeeded();
        }
}

juce::String PluginHost::formatShortName (const juce::String& f)
{
    if (f == "AudioUnit") return "AU";
    return f;
}

//==============================================================================
/** Table model for the plug-in list, drawn in the Artilody style (format badges, roomy rows). */
class StyledPluginTableModel final : public juce::TableListBoxModel
{
public:
    enum { nameCol = 1, typeCol, categoryCol, manufacturerCol, descCol }; // same ids as JUCE's columns

    StyledPluginTableModel (juce::PluginListComponent& o, juce::KnownPluginList& l) : owner (o), list (l) {}

    int getNumRows() override { return list.getNumTypes() + list.getBlacklistedFiles().size(); }

    void paintRowBackground (juce::Graphics& g, int row, int w, int h, bool selected) override
    {
        g.fillAll (selected ? Theme::selection : (row % 2 == 0 ? Theme::panel : Theme::panelAlt));
        g.setColour (Theme::border.withAlpha (0.5f));
        g.fillRect (0, h - 1, w, 1);
    }

    static juce::Colour formatColour (const juce::String& f)
    {
        if (f == "VST3")      return Theme::accentBlue;
        if (f == "AudioUnit") return Theme::accent;
        if (f == "CLAP")      return juce::Colour (0xff9b6bf2);
        return Theme::textDim;
    }

    void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool) override
    {
        const bool failed = row >= list.getNumTypes();
        auto area = juce::Rectangle<int> (0, 0, w, h).reduced (10, 0);

        if (failed)
        {
            const auto file = juce::File::createFileWithoutCheckingPath (list.getBlacklistedFiles()[row - list.getNumTypes()]);
            if (col == nameCol)
            {
                g.setColour (Theme::record);
                g.setFont (Theme::uiFont (13.0f, 600));
                g.drawFittedText (file.getFileNameWithoutExtension(), area, juce::Justification::centredLeft, 1, 0.9f);
            }
            else if (col == typeCol)
                drawBadge (g, area, "FAILED", Theme::record);
            else if (col == descCol)
            {
                g.setColour (Theme::record.withAlpha (0.8f));
                g.setFont (Theme::uiFont (12.0f));
                g.drawFittedText ("Crashed or failed to load during scanning", area, juce::Justification::centredLeft, 1, 0.9f);
            }
            return;
        }

        const auto d = list.getTypes()[row];
        switch (col)
        {
            case nameCol:
                g.setColour (Theme::text);
                g.setFont (Theme::uiFont (13.0f, 600));
                g.drawFittedText (d.name, area, juce::Justification::centredLeft, 1, 0.9f);
                break;
            case typeCol:
                drawBadge (g, area, PluginHost::formatShortName (d.pluginFormatName), formatColour (d.pluginFormatName));
                break;
            default:
            {
                juce::String text;
                if (col == categoryCol)          text = d.isInstrument ? "Instrument" : (d.category.isNotEmpty() ? d.category : "Effect");
                else if (col == manufacturerCol) text = d.manufacturerName;
                else if (col == descCol)
                {
                    juce::StringArray items;
                    if (d.descriptiveName != d.name) items.add (d.descriptiveName);
                    if (d.version.isNotEmpty())      items.add ("v" + d.version);
                    text = items.joinIntoString ("  ·  ");
                }
                g.setColour (Theme::textDim);
                g.setFont (Theme::uiFont (12.5f));
                g.drawFittedText (text, area, juce::Justification::centredLeft, 1, 0.9f);
                break;
            }
        }
    }

    void cellClicked (int row, int col, const juce::MouseEvent& e) override
    {
        juce::TableListBoxModel::cellClicked (row, col, e);
        if (row >= 0 && row < getNumRows() && e.mods.isPopupMenu())
            owner.createMenuForRow (row).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (e.originalComponent)
                                                                                  .withMousePosition()
                                                                                  .withDeletionCheck (owner));
    }

    void deleteKeyPressed (int) override { owner.removeSelectedPlugins(); }

    void sortOrderChanged (int col, bool forwards) override
    {
        using K = juce::KnownPluginList;
        switch (col)
        {
            case nameCol:         list.sort (K::sortAlphabetically, forwards); break;
            case typeCol:         list.sort (K::sortByFormat, forwards); break;
            case categoryCol:     list.sort (K::sortByCategory, forwards); break;
            case manufacturerCol: list.sort (K::sortByManufacturer, forwards); break;
            default: break;
        }
    }

private:
    static void drawBadge (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour c)
    {
        const auto font = Theme::uiFont (10.5f, 700);
        const float tw = juce::GlyphArrangement::getStringWidth (font, text) + 14.0f;
        auto pill = area.toFloat().withWidth (juce::jmin ((float) area.getWidth(), tw)).withSizeKeepingCentre (juce::jmin ((float) area.getWidth(), tw), 18.0f)
                        .withX ((float) area.getX());
        g.setColour (c.withAlpha (0.14f));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (c.darker (0.15f));
        g.setFont (font);
        g.drawText (text, pill, juce::Justification::centred, false);
    }

    juce::PluginListComponent& owner;
    juce::KnownPluginList& list;
};

//==============================================================================
/** Plug-in Manager content: header with actions, the plug-in list in a card, and a footer hint. */
class PluginManagerContent final : public juce::Component,
                                   private juce::Timer,
                                   private juce::ChangeListener
{
public:
    PluginManagerContent (juce::AudioPluginFormatManager& fm, juce::KnownPluginList& known,
                          const juce::File& deadMansPedal, juce::PropertiesFile* props)
        : formats (fm), knownList (known), properties (props),
          // Scan on the message thread (allowAsync = false, 0 threads). Many plug-ins - e.g.
          // Native Instruments' Qt based ones like Komplete Kontrol - initialise their UI
          // toolkit when the binary is loaded and assert that this happens on the main
          // thread; scanning them from a background thread crashed the app on macOS.
          list (fm, known, deadMansPedal, props, false)
    {
        list.setNumberOfThreadsForScanning (0);
        list.setScanDialogText ("Scanning plug-ins", "Looking for plug-in files...");
        list.setTableModel (new StyledPluginTableModel (list, known));
        list.getOptionsButton().setVisible (false);

        auto& table = list.getTableListBox();
        table.setRowHeight (32);
        table.setHeaderHeight (32);
        table.setOutlineThickness (0);
        table.setColour (juce::ListBox::backgroundColourId, Theme::panel);
        auto& header = table.getHeader();
        header.setColumnWidth (StyledPluginTableModel::nameCol, 230);
        header.setColumnWidth (StyledPluginTableModel::typeCol, 80);
        header.setColumnWidth (StyledPluginTableModel::categoryCol, 120);
        header.setColumnWidth (StyledPluginTableModel::manufacturerCol, 190);
        header.setStretchToFitActive (true);
        addAndMakeVisible (list);

        scanAllButton.setColour (juce::TextButton::buttonColourId, Theme::accent);
        scanAllButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        scanAllButton.setTooltip ("Scan VST3, AU and CLAP plug-ins in one go. Only new or updated plug-ins are loaded.");
        scanAllButton.onClick = [this] { scanAllFormats(); };
        addAndMakeVisible (scanAllButton);

        optionsButton.setTooltip ("Scan a single format, change scan folders, sort or clear the list");
        optionsButton.setTriggeredOnMouseDown (true);
        optionsButton.onClick = [this]
        {
            list.createOptionsMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (optionsButton)
                                                                             .withDeletionCheck (*this));
        };
        addAndMakeVisible (optionsButton);

        emptyHint.setText ("No plug-ins yet.\nClick \"Scan for new plug-ins\" to find your VST3, AU and CLAP plug-ins.",
                           juce::dontSendNotification);
        emptyHint.setJustificationType (juce::Justification::centred);
        emptyHint.setFont (Theme::uiFont (14.0f));
        emptyHint.setColour (juce::Label::textColourId, Theme::textDim);
        emptyHint.setInterceptsMouseClicks (false, false);
        addChildComponent (emptyHint);

        knownList.addChangeListener (this);
        updateSummary();
    }

    ~PluginManagerContent() override
    {
        stopTimer();
        knownList.removeChangeListener (this);
    }

    /** Scans every format that can scan for plug-ins, one after another, without folder dialogs. */
    void scanAllFormats()
    {
        if (list.isScanning() || ! queue.empty())
            return;

        for (auto* f : formats.getFormats())
            if (f->canScanForPlugins())
                queue.push_back (f);

        scanAllButton.setEnabled (false);
        scanned.clear();
        startTimer (200);
        startNextFormat();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24, 20);

        auto head = r.removeFromTop (52);
        headerText = head;
        scanAllButton.setBounds (head.removeFromRight (260).withSizeKeepingCentre (260, 36));
        head.removeFromRight (10);
        optionsButton.setBounds (head.removeFromRight (110).withSizeKeepingCentre (110, 36));

        r.removeFromTop (8);
        statusArea = r.removeFromTop (22);
        r.removeFromTop (10);

        footerArea = r.removeFromBottom (22);
        r.removeFromBottom (10);

        cardArea = r;
        list.setBounds (r.reduced (1, 1).withTrimmedTop (4).withTrimmedBottom (4));
        emptyHint.setBounds (r.withTrimmedTop (36).reduced (20));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::background);

        // header: icon tile, title and summary
        auto h = headerText;
        auto tile = h.removeFromLeft (48).withSizeKeepingCentre (44, 44).toFloat();
        juce::ColourGradient grad (Theme::accent, tile.getX(), tile.getY(), Theme::accentBlue, tile.getRight(), tile.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (tile, 11.0f);
        Theme::drawIcon (g, Theme::Icon::Plug, tile.reduced (9.0f), juce::Colours::white);
        h.removeFromLeft (14);

        g.setColour (Theme::text);
        g.setFont (Theme::uiFont (21.0f, 700));
        g.drawText ("Plug-in Manager", h.removeFromTop (28), juce::Justification::bottomLeft, true);
        g.setColour (Theme::textDim);
        g.setFont (Theme::uiFont (13.0f));
        g.drawText (summary, h, juce::Justification::topLeft, true);

        // status line with a coloured dot
        auto s = statusArea;
        const bool busy = list.isScanning() || ! queue.empty();
        g.setColour (busy ? Theme::accentBlue : Theme::accent);
        g.fillEllipse (s.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat());
        s.removeFromLeft (8);
        g.setColour (Theme::text.withAlpha (0.8f));
        g.setFont (Theme::uiFont (12.5f, 600));
        g.drawText (statusText, s, juce::Justification::centredLeft, true);

        // card behind the table
        Theme::drawCard (g, cardArea.toFloat(), 10.0f);

        g.setColour (Theme::textFaint);
        g.setFont (Theme::uiFont (12.0f));
        g.drawText ("Right-click a plug-in to remove it or show its folder.  Plug-ins that crash during scanning are marked FAILED and skipped next time.",
                    footerArea, juce::Justification::centredLeft, true);
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { updateSummary(); }

    void updateSummary()
    {
        int instruments = 0, effects = 0;
        for (auto& d : knownList.getTypes())
            (d.isInstrument ? instruments : effects)++;
        const int failed = knownList.getBlacklistedFiles().size();

        summary = juce::String (instruments + effects) + " plug-ins  ·  " + juce::String (instruments) + " instruments  ·  "
                + juce::String (effects) + " effects" + (failed > 0 ? "  ·  " + juce::String (failed) + " failed" : juce::String())
                + "      VST3 / AU / CLAP";
        emptyHint.setVisible (knownList.getNumTypes() == 0 && failed == 0);
        if (statusText.isEmpty())
            statusText = "Ready";
        repaint();
    }

    void setStatus (const juce::String& text)
    {
        statusText = text;
        repaint (statusArea);
    }

    void startNextFormat()
    {
        while (! queue.empty())
        {
            auto* format = queue.front();
            queue.erase (queue.begin());

            const auto path = properties != nullptr ? juce::PluginListComponent::getLastSearchPath (*properties, *format)
                                                    : format->getDefaultLocationsToSearch();

            setStatus ("Searching for " + PluginHost::formatShortName (format->getName()) + " plug-ins...");
            auto files = format->searchPathsForPlugins (path, true, false);
            scanned.add (PluginHost::formatShortName (format->getName()));

            // An empty list would make JUCE ask for scan folders - skip the format instead.
            if (files.isEmpty())
                continue;

            setStatus ("Scanning " + PluginHost::formatShortName (format->getName()) + " plug-ins (" + juce::String (files.size()) + " files)...");
            list.scanFor (*format, files);
            return;
        }

        stopTimer();
        scanAllButton.setEnabled (true);
        setStatus ("Scan finished: " + scanned.joinIntoString (", "));
    }

    void timerCallback() override
    {
        if (! list.isScanning())
            startNextFormat();
    }

    juce::AudioPluginFormatManager& formats;
    juce::KnownPluginList& knownList;
    juce::PropertiesFile* properties;
    juce::PluginListComponent list;
    juce::TextButton scanAllButton { "Scan for new plug-ins" }, optionsButton { "Options..." };
    juce::Label emptyHint;
    juce::Rectangle<int> headerText, statusArea, cardArea, footerArea;
    juce::String summary, statusText;
    std::vector<juce::AudioPluginFormat*> queue;
    juce::StringArray scanned;
};

class PluginManagerWindow final : public juce::DocumentWindow
{
public:
    PluginManagerWindow (juce::AudioPluginFormatManager& fm, juce::KnownPluginList& known, juce::PropertiesFile* props,
                         std::function<void()> onCloseRequested)
        : DocumentWindow ("Plug-in Manager", Theme::background, DocumentWindow::closeButton),
          onClose (std::move (onCloseRequested))
    {
        setUsingNativeTitleBar (true);
        auto deadMans = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Artilody Tracker").getChildFile ("scan-crash.txt");
        deadMans.getParentDirectory().createDirectory();
        content = new PluginManagerContent (fm, known, deadMans, props);
        content->setSize (940, 620);
        setContentOwned (content, true);
        setResizable (true, false);
        setResizeLimits (760, 460, 2400, 1800);
        centreWithSize (940, 620);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        auto cb = onClose;
        juce::MessageManager::callAsync ([cb] { if (cb) cb(); });
    }

    void scanAll() { content->scanAllFormats(); }

private:
    PluginManagerContent* content = nullptr;
    std::function<void()> onClose;
};

void PluginHost::showPluginManager (bool startScanAll)
{
    if (managerWindow == nullptr)
        managerWindow = std::make_unique<PluginManagerWindow> (formatManager, knownPlugins, properties.getUserSettings(),
                                                               [this] { managerWindow.reset(); });

    managerWindow->toFront (true);

    if (startScanAll)
        if (auto* w = dynamic_cast<PluginManagerWindow*> (managerWindow.get()))
            w->scanAll();
}

std::unique_ptr<juce::AudioPluginInstance> PluginHost::createPlugin (const juce::PluginDescription& d, double sr, int bs, juce::String& error)
{
    return formatManager.createPluginInstance (d, sr, bs, error);
}

void PluginHost::addPluginsToMenu (juce::PopupMenu& menu, bool instruments, int baseId, juce::Array<juce::PluginDescription>& out) const
{
    std::map<juce::String, std::map<juce::String, juce::Array<int>>> byFormat;

    for (auto& d : knownPlugins.getTypes())
    {
        if (d.isInstrument != instruments)
            continue;
        out.add (d);
        auto vendor = d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Unknown vendor");
        byFormat[formatShortName (d.pluginFormatName)][vendor].add (out.size() - 1);
    }

    for (const char* fmt : { "VST3", "AU", "CLAP" })
    {
        juce::PopupMenu fm;
        auto it = byFormat.find (fmt);
        if (it != byFormat.end())
        {
            for (auto& vendor : it->second)
            {
                juce::PopupMenu vm;
                for (auto idx : vendor.second)
                    vm.addItem (baseId + idx, out[idx].name);
                fm.addSubMenu (vendor.first, vm);
            }
        }
        if (fm.getNumItems() == 0)
            fm.addItem (-1, "No plug-ins found - open the Plug-in Manager", false);

       #if ! JUCE_MAC
        if (juce::String (fmt) == "AU")
            continue;
       #endif
        menu.addSubMenu (fmt, fm);
    }
}

void PluginHost::showEditor (juce::AudioProcessor* p, const juce::String& title)
{
    if (p == nullptr) return;
    auto it = windows.find (p);
    if (it != windows.end())
    {
        it->second->toFront (true);
        return;
    }
    windows[p] = std::make_unique<PluginWindow> (*this, *p, title);
}

void PluginHost::closeEditor (juce::AudioProcessor* p)
{
    auto it = windows.find (p);
    if (it != windows.end())
    {
        auto w = std::move (it->second);
        windows.erase (it);
        w.reset();
    }
}

void PluginHost::closeAllEditors()
{
    while (! windows.empty())
        closeEditor (windows.begin()->first);
}

} // namespace artilody
