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

void PluginHost::showPluginManager()
{
    if (managerWindow != nullptr)
    {
        managerWindow->toFront (true);
        return;
    }

    struct ManagerWindow final : public juce::DocumentWindow
    {
        ManagerWindow (PluginHost& h)
            : DocumentWindow ("Plug-in Manager - VST3 / AU / CLAP", juce::Colours::white, DocumentWindow::closeButton), host (h)
        {
            setUsingNativeTitleBar (true);
            auto deadMans = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("Artilody Tracker").getChildFile ("scan-crash.txt");
            deadMans.getParentDirectory().createDirectory();
            auto* list = new juce::PluginListComponent (h.formatManager, h.knownPlugins, deadMans,
                                                         h.properties.getUserSettings(), true);
            list->setSize (820, 520);
            setContentOwned (list, true);
            setResizable (true, false);
            centreWithSize (820, 520);
            setVisible (true);
        }
        void closeButtonPressed() override
        {
            auto* h = &host;
            juce::MessageManager::callAsync ([h] { h->managerWindow.reset(); });
        }
        PluginHost& host;
    };

    managerWindow = std::make_unique<ManagerWindow> (*this);
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
