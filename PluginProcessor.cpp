#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace gltch;

static juce::StringArray noteNames()
{
    juce::StringArray a;
    for (auto* n : NOTE_NAMES) a.add (n);
    return a;
}

static juce::StringArray scaleNames()
{
    juce::StringArray a;
    for (auto& s : SCALES) a.add (s.name);
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout GltchProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    using PID = juce::ParameterID;

    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "bpm", 1 }, "BPM (interne)",
                                                              juce::NormalisableRange<float> (80.f, 220.f, 1.f), 150.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "dens", 1 }, "Densite",
                                                              juce::NormalisableRange<float> (0.f, 1.f, 0.01f), .35f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "chaos", 1 }, "Chaos",
                                                              juce::NormalisableRange<float> (0.f, 1.f, 0.01f), .15f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "sw", 1 }, "Swing",
                                                              juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (PID { "root", 1 }, "Ton", noteNames(), 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (PID { "sci", 1 }, "Gamme", scaleNames(), 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "auto", 1 }, "Auto alea / 1024", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "play", 1 }, "Lecture interne", false));

    for (int i = 0; i < 6; ++i)
        p.push_back (std::make_unique<juce::AudioParameterInt> (PID { "snd" + juce::String (i), 1 },
                                                                juce::String (TRACK_NAMES[i]) + " son",
                                                                0, soundCount (i) - 1, 0));
    for (int i = 0; i < FX_COUNT; ++i)
        p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "fx" + juce::String (i), 1 },
                                                                 juce::String ("FX ") + FX_NAMES[i], false));
    return { p.begin(), p.end() };
}

GltchProcessor::GltchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GLTCH", createLayout()),
      uiRng ((uint64_t) juce::Time::currentTimeMillis() * 2654435761ull + 99)
{
    pBpm = apvts.getRawParameterValue ("bpm");
    pDens = apvts.getRawParameterValue ("dens");
    pChaos = apvts.getRawParameterValue ("chaos");
    pSw = apvts.getRawParameterValue ("sw");
    pRoot = apvts.getRawParameterValue ("root");
    pSci = apvts.getRawParameterValue ("sci");
    pAuto = apvts.getRawParameterValue ("auto");
    pPlay = apvts.getRawParameterValue ("play");
    for (int i = 0; i < FX_COUNT; ++i) pFx[i] = apvts.getRawParameterValue ("fx" + juce::String (i));
    for (int i = 0; i < 6; ++i) pSnd[i] = apvts.getRawParameterValue ("snd" + juce::String (i));

    engine.rnd2 (uiRng);
    engine.rndN (uiRng);
    engine.rndG (uiRng, .35);
    loadPresetFile();
}

void GltchProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool GltchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GltchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    Params p;
    p.bpm = pBpm->load();
    p.sw = pSw->load();
    p.chaos = pChaos->load();
    p.dens = pDens->load();
    p.root = (int) pRoot->load();
    p.sci = (int) pSci->load();
    p.autoRnd = pAuto->load() > .5f;
    uint32_t m = 0;
    for (int i = 0; i < FX_COUNT; ++i)
        if (pFx[i]->load() > .5f) m |= (1u << i);
    p.fx = m;
    for (int i = 0; i < 6; ++i) p.snd[i] = (int) pSnd[i]->load();

    Transport t;
    t.bpm = p.bpm;
    bool hostPlaying = false, havePpq = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { t.ppq = *q; havePpq = true; }
            if (auto b = pos->getBpm()) t.bpm = *b;
        }
    }
    t.mode = (hostPlaying && havePpq) ? 2 : (pPlay->load() > .5f ? 1 : 0);

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), p, t);
}

juce::AudioProcessorEditor* GltchProcessor::createEditor() { return new GltchEditor (*this); }

void GltchProcessor::setParamValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

float GltchProcessor::getParamValue (const juce::String& id) const
{
    if (auto* a = const_cast<juce::AudioProcessorValueTreeState&> (apvts).getRawParameterValue (id))
        return a->load();
    return 0.f;
}

// ------------------------------------------------------------ encodage des motifs
std::string GltchProcessor::encP() const
{
    std::string s;
    s.reserve (6144);
    for (int r = 0; r < 6; ++r)
        for (int i = 0; i < Engine::N; ++i) s.push_back ((char) ('0' + (engine.P[r][i] & 3)));
    return s;
}

std::string GltchProcessor::encN() const
{
    std::string s;
    s.reserve (6144);
    for (int r = 0; r < 6; ++r)
        for (int i = 0; i < Engine::N; ++i)
        {
            int v = std::min<int> (engine.NT[r][i], 35);
            s.push_back (v < 10 ? (char) ('0' + v) : (char) ('a' + v - 10));
        }
    return s;
}

std::string GltchProcessor::encG() const
{
    static const char* hex = "0123456789abcdef";
    std::string s;
    s.reserve (1024);
    for (int i = 0; i < Engine::N; ++i) s.push_back (hex[engine.GL[i] & 15]);
    return s;
}

static int b36 (juce::juce_wchar c)
{
    if (c >= '0' && c <= '9') return (int) (c - '0');
    if (c >= 'a' && c <= 'z') return (int) (c - 'a') + 10;
    if (c >= 'A' && c <= 'Z') return (int) (c - 'A') + 10;
    return 0;
}

void GltchProcessor::decodePatterns (const juce::String& p, const juce::String& n, const juce::String& g)
{
    if (p.length() != 6144 || n.length() != 6144 || g.length() != 1024) return;
    for (int r = 0; r < 6; ++r)
        for (int i = 0; i < Engine::N; ++i)
        {
            engine.P[r][i] = (uint8_t) juce::jlimit (0, 3, b36 (p[r * 1024 + i]));
            engine.NT[r][i] = (uint8_t) juce::jlimit (0, 35, b36 (n[r * 1024 + i]));
        }
    for (int i = 0; i < Engine::N; ++i) engine.GL[i] = (uint8_t) juce::jlimit (0, 15, b36 (g[i]));
}

void GltchProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("pat_p", juce::String (encP()), nullptr);
    st.setProperty ("pat_n", juce::String (encN()), nullptr);
    st.setProperty ("pat_g", juce::String (encG()), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void GltchProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    decodePatterns (st.getProperty ("pat_p").toString(), st.getProperty ("pat_n").toString(), st.getProperty ("pat_g").toString());
    st.removeProperty ("pat_p", nullptr);
    st.removeProperty ("pat_n", nullptr);
    st.removeProperty ("pat_g", nullptr);
    apvts.replaceState (st);
}

// ------------------------------------------------------------ presets (format compatible avec la page HTML)
juce::var GltchProcessor::encodePreset() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("v", 1);
    o->setProperty ("bpm", (double) pBpm->load());
    o->setProperty ("sw", (double) pSw->load());
    o->setProperty ("chaos", (double) pChaos->load());
    o->setProperty ("dens", (double) pDens->load());
    o->setProperty ("root", (int) pRoot->load());
    o->setProperty ("sci", (int) pSci->load());
    juce::Array<juce::var> sv;
    for (int i = 0; i < 6; ++i) sv.add ((int) pSnd[i]->load());
    o->setProperty ("sv", sv);
    o->setProperty ("p", juce::String (encP()));
    o->setProperty ("n", juce::String (encN()));
    o->setProperty ("g", juce::String (encG()));
    return juce::var (o);
}

bool GltchProcessor::presetOk (const juce::var& o)
{
    if (! o.isObject()) return false;
    return (int) o.getProperty ("v", 0) == 1
        && o.getProperty ("p", juce::var()).toString().length() == 6144
        && o.getProperty ("n", juce::var()).toString().length() == 6144
        && o.getProperty ("g", juce::var()).toString().length() == 1024
        && o.getProperty ("sv", juce::var()).isArray();
}

bool GltchProcessor::applyPresetVar (const juce::var& o)
{
    if (! presetOk (o)) return false;
    decodePatterns (o["p"].toString(), o["n"].toString(), o["g"].toString());
    if (auto* a = o["sv"].getArray())
        for (int r = 0; r < 6 && r < a->size(); ++r)
            setParamValue ("snd" + juce::String (r), (float) juce::jlimit (0, soundCount (r) - 1, (int) (*a)[r]));
    setParamValue ("root", (float) juce::jlimit (0, 11, (int) o.getProperty ("root", 0)));
    setParamValue ("sci", (float) juce::jlimit (0, 5, (int) o.getProperty ("sci", 0)));
    if (o.hasProperty ("bpm")) setParamValue ("bpm", (float) (double) o["bpm"]);
    if (o.hasProperty ("sw")) setParamValue ("sw", (float) (double) o["sw"]);
    if (o.hasProperty ("chaos")) setParamValue ("chaos", (float) (double) o["chaos"]);
    if (o.hasProperty ("dens")) setParamValue ("dens", (float) (double) o["dens"]);
    return true;
}

void GltchProcessor::storePreset (int i)
{
    presets[(size_t) i] = encodePreset();
    savePresetFile();
}

void GltchProcessor::clearPreset (int i)
{
    presets[(size_t) i] = juce::var();
    savePresetFile();
}

bool GltchProcessor::applyPreset (int i) { return applyPresetVar (presets[(size_t) i]); }

juce::String GltchProcessor::presetsAsJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("gltch1024", 1);
    juce::Array<juce::var> arr;
    for (auto& v : presets) arr.add (v.isObject() ? v : juce::var());
    root->setProperty ("presets", arr);
    return juce::JSON::toString (juce::var (root), true);
}

int GltchProcessor::importPresetsJson (const juce::String& text)
{
    auto j = juce::JSON::parse (text);
    auto* arr = j["presets"].getArray();
    if (arr == nullptr) return -1;
    int n = 0;
    for (int i = 0; i < numPresets; ++i)
    {
        if (i < arr->size() && presetOk ((*arr)[i])) { presets[(size_t) i] = (*arr)[i]; ++n; }
        else presets[(size_t) i] = juce::var();
    }
    savePresetFile();
    return n;
}

juce::File GltchProcessor::presetFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("GLTCH-1024").getChildFile ("presets.json");
}

void GltchProcessor::savePresetFile()
{
    auto f = presetFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (presetsAsJson());
}

void GltchProcessor::loadPresetFile()
{
    auto f = presetFile();
    if (f.existsAsFile()) importPresetsJson (f.loadFileAsString());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GltchProcessor(); }
