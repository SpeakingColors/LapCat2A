#include "Presets.h"
using namespace juce;

namespace presets
{
static Preset mk (const char* name, const char* type, StringArray tags, std::initializer_list<std::pair<const char*, double>> over)
{
    Preset p; p.name = name; p.type = type; p.tags = tags; p.factory = true;
    for (auto& o : over)
        for (int i = 0; i < kCount; ++i)
            if (String (o.first) == keys[i]) p.v[i] = o.second;
    return p;
}

static Preset thick (Preset p, const char* src, const char* note) { p.src = src; p.note = note; return p; }

const std::vector<Preset>& factory()
{
    static const std::vector<Preset> list {
        mk ("Start Here", "Bus", { "start", "general" }, { { "peak", 50 }, { "gain", 14 } }),
        mk ("Vocal Leveler", "Vocal", { "smooth", "tracking" }, { { "peak", 45 }, { "gain", 13 } }),
        mk ("Vocal Limit Smooth", "Vocal", { "limit", "smooth" }, { { "peak", 30 }, { "mode", 1 }, { "gain", 11 } }),
        mk ("Bright Vocal", "Vocal", { "bright", "air" }, { { "peak", 40 }, { "emph", 35 }, { "gain", 9 } }),
        mk ("De-Ess Lite", "Vocal", { "sibilance" }, { { "peak", 45 }, { "emph", 70 }, { "gain", 7 } }),
        mk ("Bass Glue", "Bass", { "glue", "even" }, { { "peak", 38 }, { "gain", 9 } }),
        mk ("Bass Hold", "Bass", { "sustain", "limit" }, { { "peak", 55 }, { "mode", 1 }, { "gain", 25 } }),
        mk ("Kick Leveler", "Drum", { "kick", "hard dance" }, { { "peak", 50 }, { "mode", 1 }, { "mix", 70 }, { "gain", 21 } }),
        mk ("Drum Bus Gentle", "Drum", { "bus", "gentle" }, { { "peak", 25 }, { "emph", 20 }, { "gain", 6 } }),
        mk ("Acoustic Guitar", "Guitar", { "acoustic" }, { { "peak", 42 }, { "emph", 15 }, { "gain", 4 } }),
        mk ("Piano Sustain", "Keys", { "sustain", "smooth" }, { { "peak", 48 }, { "gain", 7 } }),
        mk ("Chord Stab Leveler", "Synth", { "stab", "demo", "clear" }, { { "peak", 62 }, { "mode", 1 }, { "gain", 7 } }),
        mk ("Hoover Leveler", "Synth", { "hard dance", "lead" }, { { "peak", 52 }, { "mode", 1 }, { "emph", 25 }, { "gain", 20 } }),
        mk ("Mix Bus Touch", "Bus", { "mix bus", "glue" }, { { "peak", 18 }, { "emph", 10 }, { "gain", 5 } }),
        mk ("Parallel Squeeze", "Bus", { "parallel", "heavy" }, { { "peak", 75 }, { "mode", 1 }, { "mix", 35 }, { "gain", 46 } }),
        thick (mk ("Thicken Lead Gentle", "Synth", { "thicken", "supersaw", "lead" }, { { "peak", 42 }, { "gain", 13 } }), "Supersaw lead, sustained",
               "light, even body; the needle moves between about 1 and 5 dB and the notes get denser"),
        thick (mk ("Thicken Lead Heavy", "Synth", { "thicken", "supersaw", "lead", "hard dance" }, { { "peak", 58 }, { "mode", 1 }, { "gain", 22 } }), "Supersaw lead, sustained",
               "deep and tight, about 3 to 18 dB of reduction; the cell and tube stage work hard, so the lead feels thick and forward"),
        thick (mk ("Thicken Pad Glue", "Synth", { "thicken", "supersaw", "pad" }, { { "peak", 55 }, { "emph", 15 }, { "gain", 14 } }), "Supersaw pad",
               "pulls the swell together and fills the gaps between chords"),
        thick (mk ("Thicken Parallel Punch", "Synth", { "thicken", "supersaw", "stab", "parallel" }, { { "peak", 70 }, { "mode", 1 }, { "mix", 40 }, { "gain", 37 } }), "Supersaw lead, stab",
               "crushed copy under the dry stab: body and weight without losing the attack"),
    };
    return list;
}

File userFolder()
{
   #if JUCE_LINUX || JUCE_BSD
    const String xdg = SystemStats::getEnvironmentVariable ("XDG_DATA_HOME", {});
    const File base = (xdg.isNotEmpty() && File::isAbsolutePath (xdg)) ? File (xdg)
                                                                       : File::getSpecialLocation (File::userHomeDirectory).getChildFile (".local/share");
   #else
    const File base = File::getSpecialLocation (File::userApplicationDataDirectory); // %APPDATA% or ~/Library/Application Support
   #endif
    return base.getChildFile ("Studio 87").getChildFile ("LapCat2A").getChildFile ("presets");
}

bool valid (const var& o)
{
    auto* obj = o.getDynamicObject();
    if (obj == nullptr) return false;
    const var name = obj->getProperty ("name"), pv = obj->getProperty ("p");
    if (! name.isString() || name.toString().isEmpty() || name.toString().length() > 60) return false;
    auto* po = pv.getDynamicObject();
    if (po == nullptr) return false;
    for (auto& nv : po->getProperties())
        for (int i = 0; i < kCount; ++i)
            if (nv.name.toString() == keys[i])
            {
                if (! (nv.value.isDouble() || nv.value.isInt() || nv.value.isInt64())) return false;
                const double x = (double) nv.value;
                if (! std::isfinite (x) || x < lo[i] || x > hi[i]) return false;
            }
    return true;
}

Preset fromVar (const var& o)
{
    Preset p;
    p.name = o["name"].toString().substring (0, 60);
    const String t = o["type"].toString();
    p.type = "Bus";
    for (auto* ty : types) if (t == ty) p.type = t;
    if (auto* arr = o["tags"].getArray())
        for (auto& tg : *arr)
            if (tg.isString() && p.tags.size() < 8) p.tags.add (tg.toString().substring (0, 20));
    if (auto* po = o["p"].getDynamicObject())
        for (int i = 0; i < kCount; ++i)
            if (po->hasProperty (keys[i])) p.v[i] = (double) po->getProperty (keys[i]);
    return p;
}

var toVar (const Preset& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("type", p.type);
    Array<var> tg; for (auto& t : p.tags) tg.add (t);
    o->setProperty ("tags", tg);
    auto* pv = new DynamicObject();
    for (int i = 0; i < kCount; ++i) pv->setProperty (keys[i], p.v[i]);
    o->setProperty ("p", var (pv));
    return var (o);
}

static String fileNameFor (const String& name)
{
    String s = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-+.");
    s = s.trim().replaceCharacters (" ", "_");
    if (s.isEmpty()) s = "preset";
    return s.substring (0, 60) + "-" + String::toHexString (name.hashCode()) + ".json";
}

Store::Store() { reload(); }

void Store::reload()
{
    mine.clear();
    auto dir = userFolder();
    if (! dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        if (f.getSize() > 20000) continue;
        const var o = JSON::parse (f);
        if (! valid (o)) continue;
        Preset p = fromVar (o);
        bool clash = false;
        for (auto& x : factory()) if (x.name == p.name) clash = true;
        for (auto& x : mine) if (x.name == p.name) clash = true;
        if (! clash) mine.push_back (p);
    }
    std::sort (mine.begin(), mine.end(), [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

std::vector<Preset> Store::all() const
{
    std::vector<Preset> r = factory();
    r.insert (r.end(), mine.begin(), mine.end());
    return r;
}

const Preset* Store::find (const String& name) const
{
    for (auto& p : factory()) if (p.name == name) return &p;
    for (auto& p : mine) if (p.name == name) return &p;
    return nullptr;
}

bool Store::save (const Preset& p, const String& replaceName)
{
    auto dir = userFolder();
    if (! dir.createDirectory().wasOk()) return false;
    const String old = replaceName.isEmpty() ? p.name : replaceName;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        const var o = JSON::parse (f);
        if (o["name"].toString() == old || o["name"].toString() == p.name) f.deleteFile();
    }
    Preset q = p; q.factory = false;
    if (! dir.getChildFile (fileNameFor (q.name)).replaceWithText (JSON::toString (toVar (q), false))) return false;
    reload();
    return true;
}

bool Store::remove (const String& name)
{
    bool ok = false;
    for (auto& f : userFolder().findChildFiles (File::findFiles, false, "*.json"))
        if (JSON::parse (f)["name"].toString() == name) ok = f.deleteFile() || ok;
    reload();
    return ok;
}

String Store::exportJson (const std::vector<Preset>& list) const
{
    auto* root = new DynamicObject();
    root->setProperty ("lapcat2a", 1);
    Array<var> arr; for (auto& p : list) arr.add (toVar (p));
    root->setProperty ("presets", arr);
    return JSON::toString (var (root), false);
}

int Store::importJson (const String& text)
{
    const var o = JSON::parse (text);
    if (! o.isObject() || (int) o["lapcat2a"] != 1) return -1;
    auto* arr = o["presets"].getArray();
    if (arr == nullptr || arr->isEmpty() || arr->size() > 200) return -1;
    for (auto& x : *arr) if (! valid (x)) return -1;
    for (auto& x : *arr)
    {
        Preset p = fromVar (x);
        const String base = p.name;
        int n = 2;
        while (find (p.name) != nullptr) p.name = base.substring (0, 56) + " " + String (n++); // never overwrite an existing preset
        save (p);
    }
    return arr->size();
}
} // namespace presets
