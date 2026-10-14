#include "ClapHost.h"

#include <clap/clap.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

#if JUCE_LINUX || JUCE_BSD || JUCE_MAC
 #include <poll.h>
#endif

#if JUCE_MAC
extern "C" void* artilodyCreateClapParentView (int w, int h);
extern "C" void artilodyReleaseClapParentView (void* view);
#endif

namespace artilody
{
namespace clapimpl
{

//==============================================================================
static juce::File getBinaryFor (const juce::File& f)
{
   #if JUCE_MAC
    if (f.isDirectory())
    {
        auto macos = f.getChildFile ("Contents/MacOS");
        auto files = macos.findChildFiles (juce::File::findFiles, false);
        if (! files.isEmpty())
            return files.getFirst();
    }
   #endif
    return f;
}

class ClapModule
{
public:
    static std::shared_ptr<ClapModule> get (const juce::String& path)
    {
        static std::mutex m;
        static std::map<juce::String, std::weak_ptr<ClapModule>> cache;
        std::lock_guard<std::mutex> sl (m);

        if (auto existing = cache[path].lock())
            return existing;

        auto mod = std::shared_ptr<ClapModule> (new ClapModule());
        if (! mod->open (path))
            return {};

        cache[path] = mod;
        return mod;
    }

    ~ClapModule()
    {
        if (entry != nullptr && initialised)
            entry->deinit();
        lib.close();
    }

    const clap_plugin_entry_t* entry = nullptr;
    const clap_plugin_factory_t* factory = nullptr;
    juce::String path;

private:
    ClapModule() = default;

    bool open (const juce::String& p)
    {
        path = p;
        auto bin = getBinaryFor (juce::File (p));
        if (! lib.open (bin.getFullPathName()))
            return false;

        entry = reinterpret_cast<const clap_plugin_entry_t*> (lib.getFunction ("clap_entry"));
        if (entry == nullptr || ! clap_version_is_compatible (entry->clap_version))
            return false;

        if (! entry->init (p.toRawUTF8()))
            return false;

        initialised = true;
        factory = static_cast<const clap_plugin_factory_t*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));
        return factory != nullptr;
    }

    juce::DynamicLibrary lib;
    bool initialised = false;
};

static void fillDescription (juce::PluginDescription& pd, const clap_plugin_descriptor_t* d, const juce::String& path, int countInFile)
{
    pd.name = juce::String::fromUTF8 (d->name);
    pd.descriptiveName = pd.name;
    pd.pluginFormatName = ClapPluginFormat::getFormatName();
    pd.manufacturerName = d->vendor != nullptr ? juce::String::fromUTF8 (d->vendor) : juce::String();
    pd.version = d->version != nullptr ? juce::String::fromUTF8 (d->version) : juce::String();
    pd.fileOrIdentifier = path;
    pd.lastFileModTime = juce::File (path).getLastModificationTime();
    pd.lastInfoUpdateTime = juce::Time::getCurrentTime();
    pd.uniqueId = pd.deprecatedUid = juce::String::fromUTF8 (d->id).hashCode();
    pd.hasSharedContainer = countInFile > 1;
    pd.isInstrument = false;

    juce::StringArray features;
    if (d->features != nullptr)
        for (int i = 0; d->features[i] != nullptr; ++i)
            features.add (juce::String::fromUTF8 (d->features[i]));

    pd.isInstrument = features.contains (CLAP_PLUGIN_FEATURE_INSTRUMENT);
    for (auto& f : features)
        if (f != CLAP_PLUGIN_FEATURE_INSTRUMENT && f != CLAP_PLUGIN_FEATURE_AUDIO_EFFECT && f != "stereo" && f != "mono")
        {
            pd.category = f;
            break;
        }

    pd.numInputChannels = pd.isInstrument ? 0 : 2;
    pd.numOutputChannels = 2;
}

//==============================================================================
/** Simple event list backed by a byte arena so the audio thread never allocates. */
struct EventList
{
    EventList()
    {
        storage.resize (1 << 16);
        offsets.reserve (2048);
        in.ctx = this;
        in.size = [] (const clap_input_events_t* l) -> uint32_t { return (uint32_t) static_cast<EventList*> (l->ctx)->offsets.size(); };
        in.get = [] (const clap_input_events_t* l, uint32_t i) -> const clap_event_header_t*
        {
            auto* self = static_cast<EventList*> (l->ctx);
            return i < self->offsets.size() ? reinterpret_cast<const clap_event_header_t*> (self->storage.data() + self->offsets[i]) : nullptr;
        };
        out.ctx = this;
        out.try_push = [] (const clap_output_events_t* l, const clap_event_header_t* e) -> bool
        {
            return static_cast<EventList*> (l->ctx)->push (e);
        };
    }

    bool push (const clap_event_header_t* e)
    {
        const size_t sz = (e->size + 7u) & ~(size_t) 7u;
        if (used + sz > storage.size() || offsets.size() >= offsets.capacity())
            return false;
        std::memcpy (storage.data() + used, e, e->size);
        offsets.push_back ((uint32_t) used);
        used += sz;
        return true;
    }

    void clear() { offsets.clear(); used = 0; }
    uint32_t size() const { return (uint32_t) offsets.size(); }
    const clap_event_header_t* get (uint32_t i) const { return reinterpret_cast<const clap_event_header_t*> (storage.data() + offsets[i]); }

    std::vector<uint8_t> storage;
    std::vector<uint32_t> offsets;
    size_t used = 0;
    clap_input_events_t in {};
    clap_output_events_t out {};
};

class ClapInstance;

//==============================================================================
/** Owns the clap_host struct and the clap_plugin. Created before the JUCE wrapper. */
class ClapHolder final : private juce::AsyncUpdater, private juce::Timer
{
public:
    ClapHolder (std::shared_ptr<ClapModule> m) : module (std::move (m))
    {
        host.clap_version = CLAP_VERSION;
        host.host_data = this;
        host.name = "Artilody Tracker";
        host.vendor = "Artilody";
        host.url = "https://github.com/shenphenchoedron-hue/Artilody-Tracker";
        host.version = "1.0.0";
        host.get_extension = &getExtension;
        host.request_restart = [] (const clap_host_t* h) { self (h)->restartRequested = true; self (h)->triggerAsyncUpdate(); };
        host.request_process = [] (const clap_host_t*) {};
        host.request_callback = [] (const clap_host_t* h) { self (h)->callbackRequested = true; self (h)->triggerAsyncUpdate(); };
        mainThreadId = std::this_thread::get_id();
    }

    ~ClapHolder() override
    {
        cancelPendingUpdate();
        stopTimer();
        timers.clear();
        if (plugin != nullptr)
            plugin->destroy (plugin);
        plugin = nullptr;
    }

    bool create (const char* pluginId, juce::String& error)
    {
        plugin = module->factory->create_plugin (module->factory, &host, pluginId);
        if (plugin == nullptr)
        {
            error = "CLAP factory could not create plug-in";
            return false;
        }
        if (! plugin->init (plugin))
        {
            error = "CLAP plug-in failed to initialise";
            plugin->destroy (plugin);
            plugin = nullptr;
            return false;
        }

        audioPorts = static_cast<const clap_plugin_audio_ports_t*> (plugin->get_extension (plugin, CLAP_EXT_AUDIO_PORTS));
        notePorts  = static_cast<const clap_plugin_note_ports_t*>  (plugin->get_extension (plugin, CLAP_EXT_NOTE_PORTS));
        params     = static_cast<const clap_plugin_params_t*>      (plugin->get_extension (plugin, CLAP_EXT_PARAMS));
        state      = static_cast<const clap_plugin_state_t*>       (plugin->get_extension (plugin, CLAP_EXT_STATE));
        gui        = static_cast<const clap_plugin_gui_t*>         (plugin->get_extension (plugin, CLAP_EXT_GUI));
        timerExt   = static_cast<const clap_plugin_timer_support_t*> (plugin->get_extension (plugin, CLAP_EXT_TIMER_SUPPORT));
        fdExt      = static_cast<const clap_plugin_posix_fd_support_t*> (plugin->get_extension (plugin, CLAP_EXT_POSIX_FD_SUPPORT));
        latencyExt = static_cast<const clap_plugin_latency_t*>     (plugin->get_extension (plugin, CLAP_EXT_LATENCY));
        tailExt    = static_cast<const clap_plugin_tail_t*>        (plugin->get_extension (plugin, CLAP_EXT_TAIL));
        return true;
    }

    int portChannels (bool isInput, int index) const
    {
        if (audioPorts == nullptr) return 0;
        clap_audio_port_info_t info {};
        if (! audioPorts->get (plugin, (uint32_t) index, isInput, &info))
            return 0;
        return (int) info.channel_count;
    }

    int numPorts (bool isInput) const
    {
        return audioPorts != nullptr ? (int) audioPorts->count (plugin, isInput) : 0;
    }

    bool isMainThread() const { return juce::MessageManager::existsAndIsCurrentThread() || std::this_thread::get_id() == mainThreadId; }

    std::shared_ptr<ClapModule> module;
    clap_host_t host {};
    const clap_plugin_t* plugin = nullptr;
    ClapInstance* owner = nullptr;

    const clap_plugin_audio_ports_t* audioPorts = nullptr;
    const clap_plugin_note_ports_t* notePorts = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_state_t* state = nullptr;
    const clap_plugin_gui_t* gui = nullptr;
    const clap_plugin_timer_support_t* timerExt = nullptr;
    const clap_plugin_posix_fd_support_t* fdExt = nullptr;
    const clap_plugin_latency_t* latencyExt = nullptr;
    const clap_plugin_tail_t* tailExt = nullptr;

    std::atomic<bool> restartRequested { false }, callbackRequested { false }, flushRequested { false };
    std::atomic<std::thread::id> audioThreadId {};
    std::thread::id mainThreadId;

    std::function<void (int, int)> onResizeRequest;
    std::function<void()> onGuiClosed;

private:
    static ClapHolder* self (const clap_host_t* h) { return static_cast<ClapHolder*> (h->host_data); }

    void handleAsyncUpdate() override;

    //==============================================================================
    struct ClapTimer final : public juce::Timer
    {
        ClapTimer (ClapHolder& o, clap_id i) : owner (o), id (i) {}
        void timerCallback() override
        {
            if (owner.timerExt != nullptr && owner.plugin != nullptr)
                owner.timerExt->on_timer (owner.plugin, id);
        }
        ClapHolder& owner;
        clap_id id;
    };
    std::map<clap_id, std::unique_ptr<ClapTimer>> timers;
    clap_id nextTimerId = 1;
    std::map<int, clap_posix_fd_flags_t> fds;

    void timerCallback() override
    {
       #if JUCE_LINUX || JUCE_BSD || JUCE_MAC
        if (fdExt == nullptr || fds.empty()) return;
        std::vector<pollfd> pfds;
        for (auto& f : fds)
        {
            pollfd p {};
            p.fd = f.first;
            if (f.second & CLAP_POSIX_FD_READ)  p.events |= POLLIN;
            if (f.second & CLAP_POSIX_FD_WRITE) p.events |= POLLOUT;
            pfds.push_back (p);
        }
        if (::poll (pfds.data(), (nfds_t) pfds.size(), 0) > 0)
        {
            for (auto& p : pfds)
            {
                clap_posix_fd_flags_t fl = 0;
                if (p.revents & POLLIN)  fl |= CLAP_POSIX_FD_READ;
                if (p.revents & POLLOUT) fl |= CLAP_POSIX_FD_WRITE;
                if (p.revents & (POLLERR | POLLHUP)) fl |= CLAP_POSIX_FD_ERROR;
                if (fl != 0)
                    fdExt->on_fd (plugin, p.fd, fl);
            }
        }
       #endif
    }

    //==============================================================================
    static const void* getExtension (const clap_host_t*, const char* id)
    {
        const juce::String ext (id);

        static const clap_host_log_t logExt { [] (const clap_host_t*, clap_log_severity sev, const char* msg)
        {
            juce::ignoreUnused (sev);
            DBG ("[CLAP] " << msg);
        } };

        static const clap_host_thread_check_t threadExt {
            [] (const clap_host_t* h) -> bool { return self (h)->isMainThread(); },
            [] (const clap_host_t* h) -> bool { return ! self (h)->isMainThread(); }
        };

        static const clap_host_gui_t guiExt {
            [] (const clap_host_t*) {},
            [] (const clap_host_t* h, uint32_t w, uint32_t hgt) -> bool
            {
                auto* s = self (h);
                juce::MessageManager::callAsync ([s, w, hgt] { if (s->onResizeRequest) s->onResizeRequest ((int) w, (int) hgt); });
                return true;
            },
            [] (const clap_host_t*) -> bool { return false; },
            [] (const clap_host_t*) -> bool { return false; },
            [] (const clap_host_t* h, bool) { auto* s = self (h); juce::MessageManager::callAsync ([s] { if (s->onGuiClosed) s->onGuiClosed(); }); }
        };

        static const clap_host_timer_support_t timerSupport {
            [] (const clap_host_t* h, uint32_t periodMs, clap_id* timerId) -> bool
            {
                auto* s = self (h);
                const auto id = s->nextTimerId++;
                auto t = std::make_unique<ClapTimer> (*s, id);
                t->startTimer ((int) juce::jmax (10u, periodMs));
                s->timers[id] = std::move (t);
                *timerId = id;
                return true;
            },
            [] (const clap_host_t* h, clap_id timerId) -> bool
            {
                return self (h)->timers.erase (timerId) > 0;
            }
        };

        static const clap_host_posix_fd_support_t fdSupport {
            [] (const clap_host_t* h, int fd, clap_posix_fd_flags_t flags) -> bool
            {
                auto* s = self (h);
                s->fds[fd] = flags;
                if (! s->isTimerRunning()) s->startTimer (10);
                return true;
            },
            [] (const clap_host_t* h, int fd, clap_posix_fd_flags_t flags) -> bool
            {
                self (h)->fds[fd] = flags;
                return true;
            },
            [] (const clap_host_t* h, int fd) -> bool
            {
                auto* s = self (h);
                s->fds.erase (fd);
                if (s->fds.empty()) s->stopTimer();
                return true;
            }
        };

        static const clap_host_params_t paramsExt {
            [] (const clap_host_t*, clap_param_rescan_flags) {},
            [] (const clap_host_t*, clap_id, clap_param_clear_flags) {},
            [] (const clap_host_t* h) { self (h)->flushRequested = true; self (h)->triggerAsyncUpdate(); }
        };

        static const clap_host_state_t stateExt { [] (const clap_host_t*) {} };
        static const clap_host_latency_t latencyExt { [] (const clap_host_t*) {} };
        static const clap_host_tail_t tailExt { [] (const clap_host_t*) {} };
        static const clap_host_audio_ports_t audioPortsExt {
            [] (const clap_host_t*, uint32_t) -> bool { return false; },
            [] (const clap_host_t*, uint32_t) {}
        };
        static const clap_host_note_ports_t notePortsExt {
            [] (const clap_host_t*) -> uint32_t { return CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI; },
            [] (const clap_host_t*, uint32_t) {}
        };

        if (ext == CLAP_EXT_LOG)              return &logExt;
        if (ext == CLAP_EXT_THREAD_CHECK)     return &threadExt;
        if (ext == CLAP_EXT_GUI)              return &guiExt;
        if (ext == CLAP_EXT_TIMER_SUPPORT)    return &timerSupport;
        if (ext == CLAP_EXT_POSIX_FD_SUPPORT) return &fdSupport;
        if (ext == CLAP_EXT_PARAMS)           return &paramsExt;
        if (ext == CLAP_EXT_STATE)            return &stateExt;
        if (ext == CLAP_EXT_LATENCY)          return &latencyExt;
        if (ext == CLAP_EXT_TAIL)             return &tailExt;
        if (ext == CLAP_EXT_AUDIO_PORTS)      return &audioPortsExt;
        if (ext == CLAP_EXT_NOTE_PORTS)       return &notePortsExt;
        return nullptr;
    }
};

//==============================================================================
class ClapParameter final : public juce::AudioPluginInstance::HostedParameter
{
public:
    ClapParameter (ClapHolder& h, const clap_param_info_t& i, double current)
        : holder (h), info (i), plainValue (current) {}

    float getValue() const override             { return toNormal (plainValue.load()); }
    void setValue (float v) override
    {
        auto plain = fromNormal (v);
        plainValue = plain;
        pending = true;
    }
    float getDefaultValue() const override      { return toNormal (info.default_value); }
    juce::String getName (int maxLen) const override { return juce::String::fromUTF8 (info.name).substring (0, maxLen); }
    juce::String getLabel() const override      { return {}; }
    juce::String getParameterID() const override { return juce::String ((juce::int64) info.id); }
    bool isDiscrete() const override            { return (info.flags & CLAP_PARAM_IS_STEPPED) != 0; }
    int getNumSteps() const override
    {
        if (isDiscrete())
            return juce::jmax (2, (int) std::round (info.max_value - info.min_value) + 1);
        return juce::AudioProcessor::getDefaultNumParameterSteps();
    }
    bool isAutomatable() const override         { return (info.flags & CLAP_PARAM_IS_AUTOMATABLE) != 0; }

    juce::String getText (float normal, int maxLen) const override
    {
        const double plain = fromNormal (normal);
        if (holder.params != nullptr && holder.isMainThread())
        {
            char buf[256] = {};
            if (holder.params->value_to_text (holder.plugin, info.id, plain, buf, sizeof (buf)))
                return juce::String::fromUTF8 (buf).substring (0, maxLen);
        }
        return juce::String (plain, 3).substring (0, maxLen);
    }

    float getValueForText (const juce::String& text) const override
    {
        double v = text.getDoubleValue();
        if (holder.params != nullptr && holder.isMainThread())
            holder.params->text_to_value (holder.plugin, info.id, text.toRawUTF8(), &v);
        return toNormal (v);
    }

    float toNormal (double plain) const
    {
        const double range = info.max_value - info.min_value;
        return range > 0 ? (float) juce::jlimit (0.0, 1.0, (plain - info.min_value) / range) : 0.0f;
    }

    double fromNormal (float n) const
    {
        double v = info.min_value + (double) n * (info.max_value - info.min_value);
        if (isDiscrete()) v = std::round (v);
        return v;
    }

    ClapHolder& holder;
    clap_param_info_t info;
    std::atomic<double> plainValue;
    std::atomic<bool> pending { false };
};

//==============================================================================
class ClapInstance final : public juce::AudioPluginInstance
{
public:
    static BusesProperties makeBuses (ClapHolder& h)
    {
        BusesProperties b;
        auto setFor = [] (int n) { return n == 1 ? juce::AudioChannelSet::mono() : (n == 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::discreteChannels (n)); };
        const int in = h.numPorts (true) > 0 ? h.portChannels (true, 0) : 0;
        const int out = h.numPorts (false) > 0 ? h.portChannels (false, 0) : 0;
        if (in > 0)  b = b.withInput ("Input", setFor (in), true);
        if (out > 0) b = b.withOutput ("Output", setFor (out), true);
        return b;
    }

    ClapInstance (std::unique_ptr<ClapHolder> h, const juce::PluginDescription& d)
        : AudioPluginInstance (makeBuses (*h)), holder (std::move (h)), description (d)
    {
        holder->owner = this;

        // note dialect
        if (holder->notePorts != nullptr && holder->notePorts->count (holder->plugin, true) > 0)
        {
            clap_note_port_info_t np {};
            if (holder->notePorts->get (holder->plugin, 0, true, &np))
            {
                wantsMidiDialect = (np.supported_dialects & CLAP_NOTE_DIALECT_CLAP) == 0;
                hasNoteInput = true;
            }
        }

        // parameters
        if (holder->params != nullptr)
        {
            const auto count = holder->params->count (holder->plugin);
            for (uint32_t i = 0; i < count; ++i)
            {
                clap_param_info_t info {};
                if (! holder->params->get_info (holder->plugin, i, &info))
                    continue;
                if (info.flags & CLAP_PARAM_IS_HIDDEN)
                    continue;
                double v = info.default_value;
                holder->params->get_value (holder->plugin, info.id, &v);
                auto p = std::make_unique<ClapParameter> (*holder, info, v);
                paramById[info.id] = p.get();
                clapParams.push_back (p.get());
                addHostedParameter (std::move (p));
            }
        }
    }

    ~ClapInstance() override
    {
        deactivateSafely();
        holder.reset();
    }

    ClapHolder& getHolder() { return *holder; }

    //==============================================================================
    void fillInPluginDescription (juce::PluginDescription& d) const override { d = description; }
    const juce::String getName() const override { return description.name; }
    bool acceptsMidi() const override  { return hasNoteInput || description.isInstrument; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override
    {
        if (holder->tailExt != nullptr && activated)
        {
            const auto t = holder->tailExt->get (holder->plugin);
            return t == UINT32_MAX ? 10.0 : (double) t / juce::jmax (1.0, activeRate);
        }
        return 0.0;
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l == getBusesLayout(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void prepareToPlay (double sr, int bs) override
    {
        if (activated && (! juce::approximatelyEqual (sr, activeRate) || bs > activeMax))
            deactivateSafely();

        allocatePorts (bs);

        if (! activated)
        {
            activeRate = sr;
            activeMax = bs;
            activated = holder->plugin->activate (holder->plugin, sr, 1, (uint32_t) bs);
        }
    }

    void releaseResources() override
    {
        deactivateSafely();
    }

    void deactivateSafely()
    {
        if (! activated) return;
        const juce::ScopedLock sl (getCallbackLock());
        if (processing)
        {
            holder->plugin->stop_processing (holder->plugin);
            processing = false;
        }
        holder->plugin->deactivate (holder->plugin);
        activated = false;
    }

    void restart()
    {
        const auto sr = activeRate;
        const auto bs = activeMax;
        deactivateSafely();
        if (sr > 0)
            prepareToPlay (sr, bs);
    }

    void allocatePorts (int bs)
    {
        auto alloc = [bs] (std::vector<juce::AudioBuffer<float>>& bufs, std::vector<clap_audio_buffer_t>& cbs, std::vector<std::vector<float*>>& ptrs, ClapHolder& h, bool input)
        {
            const int n = h.numPorts (input);
            bufs.resize ((size_t) n);
            cbs.resize ((size_t) n);
            ptrs.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const int ch = juce::jmax (1, h.portChannels (input, i));
                bufs[(size_t) i].setSize (ch, juce::jmax (bs, 16));
                bufs[(size_t) i].clear();
                ptrs[(size_t) i].assign ((size_t) ch, nullptr);
                for (int c = 0; c < ch; ++c)
                    ptrs[(size_t) i][(size_t) c] = bufs[(size_t) i].getWritePointer (c);
                auto& cb = cbs[(size_t) i];
                cb.data32 = ptrs[(size_t) i].data();
                cb.data64 = nullptr;
                cb.channel_count = (uint32_t) ch;
                cb.latency = 0;
                cb.constant_mask = 0;
            }
        };
        const juce::ScopedLock sl (getCallbackLock());
        alloc (inBufs, inClap, inPtrs, *holder, true);
        alloc (outBufs, outClap, outPtrs, *holder, false);
    }

    //==============================================================================
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        const int n = buffer.getNumSamples();
        holder->audioThreadId = std::this_thread::get_id();

        if (! activated || (! outBufs.empty() && n > outBufs[0].getNumSamples()))
        {
            buffer.clear();
            return;
        }

        if (! processing)
        {
            processing = holder->plugin->start_processing (holder->plugin);
            if (! processing)
            {
                buffer.clear();
                return;
            }
        }

        // audio inputs
        for (size_t p = 0; p < inBufs.size(); ++p)
        {
            auto& b = inBufs[p];
            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                if (p == 0 && c < buffer.getNumChannels() && c < getTotalNumInputChannels())
                    juce::FloatVectorOperations::copy (inPtrs[p][(size_t) c], buffer.getReadPointer (c), n);
                else
                    juce::FloatVectorOperations::clear (inPtrs[p][(size_t) c], n);
            }
        }
        for (auto& chans : outPtrs)
            for (auto* ptr : chans)
                juce::FloatVectorOperations::clear (ptr, n);

        // events
        inEvents.clear();
        outEvents.clear();

        for (auto* p : clapParams)
        {
            if (! p->pending.exchange (false)) continue;
            clap_event_param_value_t ev {};
            ev.header.size = sizeof (ev);
            ev.header.time = 0;
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.type = CLAP_EVENT_PARAM_VALUE;
            ev.header.flags = 0;
            ev.param_id = p->info.id;
            ev.cookie = p->info.cookie;
            ev.note_id = -1;
            ev.port_index = -1;
            ev.channel = -1;
            ev.key = -1;
            ev.value = p->plainValue.load();
            inEvents.push (&ev.header);
        }

        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const auto time = (uint32_t) juce::jlimit (0, n - 1, meta.samplePosition);
            if (! wantsMidiDialect && (m.isNoteOn() || m.isNoteOff()))
            {
                clap_event_note_t ev {};
                ev.header.size = sizeof (ev);
                ev.header.time = time;
                ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                ev.header.type = m.isNoteOn() ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF;
                ev.note_id = -1;
                ev.port_index = 0;
                ev.channel = (int16_t) (m.getChannel() - 1);
                ev.key = (int16_t) m.getNoteNumber();
                ev.velocity = m.isNoteOn() ? (double) m.getFloatVelocity() : 0.0;
                inEvents.push (&ev.header);
            }
            else if (m.getRawDataSize() <= 3)
            {
                clap_event_midi_t ev {};
                ev.header.size = sizeof (ev);
                ev.header.time = time;
                ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                ev.header.type = CLAP_EVENT_MIDI;
                ev.port_index = 0;
                const auto* raw = m.getRawData();
                for (int i = 0; i < m.getRawDataSize(); ++i)
                    ev.data[i] = raw[i];
                inEvents.push (&ev.header);
            }
        }

        // transport
        clap_event_transport_t transport {};
        transport.header.size = sizeof (transport);
        transport.header.type = CLAP_EVENT_TRANSPORT;
        transport.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        transport.tempo = 120.0;
        transport.tsig_num = 4;
        transport.tsig_denom = 4;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
                if (pos->getIsPlaying()) transport.flags |= CLAP_TRANSPORT_IS_PLAYING;
                transport.tempo = pos->getBpm().orFallback (120.0);
                const double ppq = pos->getPpqPosition().orFallback (0.0);
                transport.song_pos_beats = (clap_beattime) std::llround (ppq * (double) CLAP_BEATTIME_FACTOR);
                transport.song_pos_seconds = (clap_sectime) std::llround (pos->getTimeInSeconds().orFallback (0.0) * (double) CLAP_SECTIME_FACTOR);
                const double bar = pos->getPpqPositionOfLastBarStart().orFallback (0.0);
                transport.bar_start = (clap_beattime) std::llround (bar * (double) CLAP_BEATTIME_FACTOR);
                transport.bar_number = (int32_t) (bar / 4.0);
            }
        }

        clap_process_t proc {};
        proc.steady_time = steadyTime;
        proc.frames_count = (uint32_t) n;
        proc.transport = &transport;
        proc.audio_inputs = inClap.data();
        proc.audio_inputs_count = (uint32_t) inClap.size();
        proc.audio_outputs = outClap.data();
        proc.audio_outputs_count = (uint32_t) outClap.size();
        proc.in_events = &inEvents.in;
        proc.out_events = &outEvents.out;

        holder->plugin->process (holder->plugin, &proc);
        steadyTime += n;

        // copy main output
        const int outCh = getTotalNumOutputChannels();
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            if (! outBufs.empty() && c < outCh && c < outBufs[0].getNumChannels())
                buffer.copyFrom (c, 0, outPtrs[0][(size_t) c], n);
            else
                buffer.clear (c, 0, n);
        }

        // parameter changes reported by the plug-in
        for (uint32_t i = 0; i < outEvents.size(); ++i)
        {
            auto* e = outEvents.get (i);
            if (e->space_id == CLAP_CORE_EVENT_SPACE_ID && e->type == CLAP_EVENT_PARAM_VALUE)
            {
                auto* pv = reinterpret_cast<const clap_event_param_value_t*> (e);
                auto it = paramById.find (pv->param_id);
                if (it != paramById.end())
                {
                    it->second->plainValue = pv->value;
                    it->second->sendValueChangedMessageToListeners (it->second->toNormal (pv->value));
                }
            }
        }
    }

    /** Main-thread parameter flush when audio isn't running. */
    void flushParams()
    {
        if (holder->params == nullptr || processing) return;
        EventList in, out;
        for (auto* p : clapParams)
        {
            if (! p->pending.exchange (false)) continue;
            clap_event_param_value_t ev {};
            ev.header.size = sizeof (ev);
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.type = CLAP_EVENT_PARAM_VALUE;
            ev.param_id = p->info.id;
            ev.cookie = p->info.cookie;
            ev.note_id = -1; ev.port_index = -1; ev.channel = -1; ev.key = -1;
            ev.value = p->plainValue.load();
            in.push (&ev.header);
        }
        holder->params->flush (holder->plugin, &in.in, &out.out);
        refreshParamValues();
    }

    void refreshParamValues()
    {
        if (holder->params == nullptr) return;
        for (auto* p : clapParams)
        {
            double v = p->plainValue.load();
            if (holder->params->get_value (holder->plugin, p->info.id, &v))
                p->plainValue = v;
        }
    }

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& mb) override
    {
        mb.reset();
        if (holder->state == nullptr) return;
        juce::MemoryOutputStream os (mb, false);
        clap_ostream_t s {};
        s.ctx = &os;
        s.write = [] (const clap_ostream_t* st, const void* buf, uint64_t size) -> int64_t
        {
            static_cast<juce::MemoryOutputStream*> (st->ctx)->write (buf, (size_t) size);
            return (int64_t) size;
        };
        holder->state->save (holder->plugin, &s);
        os.flush();
    }

    void setStateInformation (const void* data, int size) override
    {
        if (holder->state == nullptr) return;
        juce::MemoryInputStream is (data, (size_t) size, false);
        clap_istream_t s {};
        s.ctx = &is;
        s.read = [] (const clap_istream_t* st, void* buf, uint64_t sz) -> int64_t
        {
            return (int64_t) static_cast<juce::MemoryInputStream*> (st->ctx)->read (buf, (int) sz);
        };
        holder->state->load (holder->plugin, &s);
        refreshParamValues();
    }

    //==============================================================================
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

private:
    std::unique_ptr<ClapHolder> holder;
    juce::PluginDescription description;
    bool wantsMidiDialect = false, hasNoteInput = false;

    std::vector<ClapParameter*> clapParams;
    std::unordered_map<clap_id, ClapParameter*> paramById;

    std::atomic<bool> activated { false };
    bool processing = false;
    double activeRate = 0.0;
    int activeMax = 0;
    int64_t steadyTime = 0;

    std::vector<juce::AudioBuffer<float>> inBufs, outBufs;
    std::vector<clap_audio_buffer_t> inClap, outClap;
    std::vector<std::vector<float*>> inPtrs, outPtrs;
    EventList inEvents, outEvents;
};

void ClapHolder::handleAsyncUpdate()
{
    if (plugin == nullptr) return;

    if (callbackRequested.exchange (false))
        plugin->on_main_thread (plugin);

    if (restartRequested.exchange (false) && owner != nullptr)
        owner->restart();

    if (flushRequested.exchange (false) && owner != nullptr)
        owner->flushParams();
}

//==============================================================================
static const char* nativeApi()
{
   #if JUCE_WINDOWS
    return CLAP_WINDOW_API_WIN32;
   #elif JUCE_MAC
    return CLAP_WINDOW_API_COCOA;
   #else
    return CLAP_WINDOW_API_X11;
   #endif
}

#if JUCE_WINDOWS
struct NativeChild final : public juce::HWNDComponent
{
    NativeChild()
    {
        setOpaque (true);
        inner.addToDesktop (0);
        if (auto* peer = inner.getPeer())
            setHWND (peer->getNativeHandle());
    }
    void* getNativeParent() { return getHWND(); }

    struct Inner final : public juce::Component
    {
        Inner() { setOpaque (true); }
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    };
    Inner inner;
};
#elif JUCE_MAC
struct NativeChild final : public juce::NSViewComponent
{
    NativeChild()
    {
        view = artilodyCreateClapParentView (400, 300);
        setView (view);
    }
    ~NativeChild() override
    {
        setView (nullptr);
        artilodyReleaseClapParentView (view);
    }
    void* getNativeParent() { return view; }
    void* view = nullptr;
};
#else
struct NativeChild final : public juce::XEmbedComponent
{
    NativeChild() : XEmbedComponent (true, false) {}
    unsigned long getNativeParent() { return getHostWindowID(); }
};
#endif

class ClapEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    ClapEditor (ClapInstance& inst) : AudioProcessorEditor (inst), owner (inst), holder (inst.getHolder())
    {
        auto* gui = holder.gui;
        floating = ! gui->is_api_supported (holder.plugin, nativeApi(), false);

        if (! gui->create (holder.plugin, nativeApi(), floating))
        {
            failed = true;
            setSize (360, 80);
            return;
        }
        created = true;

       #if ! JUCE_MAC
        gui->set_scale (holder.plugin, 1.0);
       #endif

        uint32_t w = 400, h = 300;
        gui->get_size (holder.plugin, &w, &h);

        holder.onResizeRequest = [safe = juce::Component::SafePointer<ClapEditor> (this)] (int nw, int nh)
        {
            if (safe != nullptr)
            {
                safe->ignoreResize = true;
                safe->setSize (nw, nh);
                safe->ignoreResize = false;
            }
        };
        holder.onGuiClosed = [safe = juce::Component::SafePointer<ClapEditor> (this)]
        {
            if (safe != nullptr)
                if (auto* w = safe->findParentComponentOfClass<juce::DocumentWindow>())
                    w->closeButtonPressed();
        };

        if (floating)
        {
            gui->suggest_title (holder.plugin, owner.getName().toRawUTF8());
            gui->show (holder.plugin);
            setSize (360, 80);
        }
        else
        {
            addAndMakeVisible (child);
            setResizable (gui->can_resize (holder.plugin), false);
            setSize ((int) juce::jmax (50u, w), (int) juce::jmax (50u, h));
            startTimer (30);
        }
    }

    ~ClapEditor() override
    {
        stopTimer();
        holder.onResizeRequest = nullptr;
        holder.onGuiClosed = nullptr;
        if (created)
        {
            holder.gui->hide (holder.plugin);
            holder.gui->destroy (holder.plugin);
        }
        owner.editorBeingDeleted (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b2433));
        if (failed || floating)
        {
            g.setColour (juce::Colours::white);
            g.drawFittedText (failed ? "This CLAP plug-in could not open its editor."
                                     : "The plug-in editor is open in its own window.",
                              getLocalBounds().reduced (10), juce::Justification::centred, 2);
        }
    }

    void resized() override
    {
        child.setBounds (getLocalBounds());
        if (attached && ! ignoreResize && holder.gui->can_resize (holder.plugin))
        {
            uint32_t w = (uint32_t) getWidth(), h = (uint32_t) getHeight();
            holder.gui->adjust_size (holder.plugin, &w, &h);
            holder.gui->set_size (holder.plugin, w, h);
        }
       #if JUCE_LINUX || JUCE_BSD
        child.updateEmbeddedBounds();
       #endif
    }

private:
    void timerCallback() override
    {
        if (attached || ! isShowing() || getPeer() == nullptr)
            return;

        clap_window_t win {};
        win.api = nativeApi();
       #if JUCE_WINDOWS
        win.win32 = child.getNativeParent();
       #elif JUCE_MAC
        win.cocoa = child.getNativeParent();
       #else
        win.x11 = (clap_xwnd) child.getNativeParent();
       #endif

        if (holder.gui->set_parent (holder.plugin, &win))
        {
            holder.gui->show (holder.plugin);
            attached = true;
        }
        stopTimer();
    }

    ClapInstance& owner;
    ClapHolder& holder;
    NativeChild child;
    bool floating = false, failed = false, created = false, attached = false, ignoreResize = false;
};

bool ClapInstance::hasEditor() const
{
    return holder->gui != nullptr
        && (holder->gui->is_api_supported (holder->plugin, nativeApi(), false)
            || holder->gui->is_api_supported (holder->plugin, nativeApi(), true));
}

juce::AudioProcessorEditor* ClapInstance::createEditor()
{
    if (! hasEditor()) return nullptr;
    return new ClapEditor (*this);
}

} // namespace clapimpl

//==============================================================================
using namespace clapimpl;

bool ClapPluginFormat::fileMightContainThisPluginType (const juce::String& f)
{
    juce::File file (f);
   #if JUCE_MAC
    return file.hasFileExtension (".clap") && file.isDirectory();
   #else
    return file.hasFileExtension (".clap") && file.existsAsFile();
   #endif
}

juce::String ClapPluginFormat::getNameOfPluginFromIdentifier (const juce::String& f)
{
    return juce::File (f).getFileNameWithoutExtension();
}

bool ClapPluginFormat::pluginNeedsRescanning (const juce::PluginDescription& d)
{
    return juce::File (d.fileOrIdentifier).getLastModificationTime() != d.lastFileModTime;
}

bool ClapPluginFormat::doesPluginStillExist (const juce::PluginDescription& d)
{
    return juce::File (d.fileOrIdentifier).exists();
}

void ClapPluginFormat::findAllTypesForFile (juce::OwnedArray<juce::PluginDescription>& results, const juce::String& path)
{
    if (! fileMightContainThisPluginType (path))
        return;

    auto mod = ClapModule::get (path);
    if (mod == nullptr || mod->factory == nullptr)
        return;

    const auto count = (int) mod->factory->get_plugin_count (mod->factory);
    for (int i = 0; i < count; ++i)
    {
        auto* d = mod->factory->get_plugin_descriptor (mod->factory, (uint32_t) i);
        if (d == nullptr || ! clap_version_is_compatible (d->clap_version))
            continue;
        auto pd = std::make_unique<juce::PluginDescription>();
        fillDescription (*pd, d, path, count);
        results.add (pd.release());
    }
}

juce::StringArray ClapPluginFormat::searchPathsForPlugins (const juce::FileSearchPath& dirs, bool recursive, bool)
{
    juce::StringArray results;
    std::function<void (const juce::File&, int)> scan = [&] (const juce::File& dir, int depth)
    {
        if (depth > 8) return;
        for (auto& child : dir.findChildFiles (juce::File::findFilesAndDirectories, false))
        {
            if (child.hasFileExtension (".clap"))
                results.add (child.getFullPathName());
            else if (recursive && child.isDirectory())
                scan (child, depth + 1);
        }
    };
    for (int i = 0; i < dirs.getNumPaths(); ++i)
        scan (dirs[i], 0);
    return results;
}

juce::FileSearchPath ClapPluginFormat::getDefaultLocationsToSearch()
{
    juce::FileSearchPath p;
   #if JUCE_WINDOWS
    p.add (juce::File (juce::SystemStats::getEnvironmentVariable ("COMMONPROGRAMFILES", "C:\\Program Files\\Common Files")).getChildFile ("CLAP"));
    p.add (juce::File (juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", "")).getChildFile ("Programs\\Common\\CLAP"));
    const char* sep = ";";
   #elif JUCE_MAC
    p.add (juce::File ("/Library/Audio/Plug-Ins/CLAP"));
    p.add (juce::File ("~/Library/Audio/Plug-Ins/CLAP"));
    const char* sep = ":";
   #else
    p.add (juce::File ("~/.clap"));
    p.add (juce::File ("/usr/lib/clap"));
    p.add (juce::File ("/usr/local/lib/clap"));
    const char* sep = ":";
   #endif
    for (auto& e : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("CLAP_PATH", ""), sep, ""))
        if (e.isNotEmpty())
            p.add (juce::File (e));
    p.removeRedundantPaths();
    return p;
}

void ClapPluginFormat::createPluginInstance (const juce::PluginDescription& desc, double sr, int bs, PluginCreationCallback callback)
{
    juce::String error;
    auto mod = ClapModule::get (desc.fileOrIdentifier);
    if (mod == nullptr || mod->factory == nullptr)
    {
        callback (nullptr, "Could not load CLAP module " + desc.fileOrIdentifier);
        return;
    }

    const clap_plugin_descriptor_t* found = nullptr;
    const auto count = mod->factory->get_plugin_count (mod->factory);
    for (uint32_t i = 0; i < count; ++i)
    {
        auto* d = mod->factory->get_plugin_descriptor (mod->factory, i);
        if (d != nullptr && juce::String::fromUTF8 (d->id).hashCode() == desc.uniqueId)
        {
            found = d;
            break;
        }
    }

    if (found == nullptr)
    {
        callback (nullptr, "CLAP plug-in not found in " + desc.fileOrIdentifier);
        return;
    }

    auto holder = std::make_unique<ClapHolder> (mod);
    if (! holder->create (found->id, error))
    {
        callback (nullptr, error);
        return;
    }

    juce::PluginDescription pd;
    fillDescription (pd, found, desc.fileOrIdentifier, (int) count);
    auto inst = std::make_unique<ClapInstance> (std::move (holder), pd);
    inst->setRateAndBufferSizeDetails (sr, bs);
    callback (std::move (inst), {});
}

} // namespace artilody
