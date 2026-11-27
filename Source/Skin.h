#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

// ============================================================================
//  Skin = up to 4 stacked VFX layers + theme colours. Everything is editable
//  and saved inside the plugin state (so it travels with the FL project) and
//  can also be saved as a .mvxskin file or set as your default skin.
// ============================================================================
enum VfxType {
    VfxNone = 0, VfxGalaxy, VfxMatrix, VfxFire, VfxPyro, VfxLightning, VfxWater, VfxTropical,
    VfxWisdomTree, VfxNeonNotes, VfxStudio, VfxCar, VfxAurora, VfxStarfield, VfxMedia, VfxGradient,
    VfxNumTypes
};

inline const char* const kVfxNames[VfxNumTypes] = {
    "Off", "Galaxy", "Matrix Code", "Fire", "Pyro Fireworks", "Lightning / Thunder", "Water Ripples",
    "Tropical Island", "Wisdom Trees", "Neon Notes (Keysight)", "Chicago Studio Orbit",
    "Concept Car (Dub)", "Aurora", "Starfield Warp", "Image / GIF / SVG", "Gradient"
};

// what the "Variant" slider does for each effect (shown as a hint in the skin tab)
inline const char* const kVfxVariantHint[VfxNumTypes] = {
    "", "0-14: arm twist + tilt", "0 katakana, 1 binary, 2 hex, 3 symbols, 4 letters (repeats up)",
    "0 full wall, 1 bonfire, 2 side walls, 3 embers", "0 sphere, 1 ring, 2 willow, 3 rainbow",
    "0 storm, 1 triple, 2 plasma ball, 3 tesla field", "0 drops, 1 perspective, 2 rain, 3 plays notes",
    "0 day, 1 sunset, 2 night, 3 dusk", "0 pine 1 cedar 2 oak 3 ginkgo 4 fig 5 beech 6 willow",
    "0 neon outline, 1 solid glow, 2 teal ornate", "0 orbit outside, 1 inside, 2 top-down helix",
    "view 0 side, 1 interior TV, 2 trunk subs  |  +3 spinner rims, +6 floater rims",
    "0-9: ribbon count/shape", "0 warp, 1 snow, 2 spiral", "fit: 0 fill, 1 fit, 2 stretch, 3 tile", "0-4: direction/style"
};

constexpr int kNumLayers = 4;

struct SkinLayer
{
    bool enabled = false;
    int type = VfxNone, variant = 0;
    juce::Colour c1{ 0xffff2020 }, c2{ 0xffff8a00 }, c3{ 0xff550000 };
    bool rainbow = false;
    float speed = 1.f, intensity = 0.8f, scale = 0.5f, density = 0.5f, opacity = 1.f, react = 0.6f;
    juce::String text, path;

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto e = std::make_unique<juce::XmlElement>("Layer");
        e->setAttribute("enabled", enabled); e->setAttribute("type", type); e->setAttribute("variant", variant);
        e->setAttribute("c1", c1.toString()); e->setAttribute("c2", c2.toString()); e->setAttribute("c3", c3.toString());
        e->setAttribute("rainbow", rainbow);
        e->setAttribute("speed", (double)speed); e->setAttribute("intensity", (double)intensity);
        e->setAttribute("scale", (double)scale); e->setAttribute("density", (double)density);
        e->setAttribute("opacity", (double)opacity); e->setAttribute("react", (double)react);
        e->setAttribute("text", text); e->setAttribute("path", path);
        return e;
    }
    void fromXml(const juce::XmlElement& e)
    {
        enabled = e.getBoolAttribute("enabled", false);
        type = juce::jlimit(0, (int)VfxNumTypes - 1, e.getIntAttribute("type", 0));
        variant = juce::jmax(0, e.getIntAttribute("variant", 0));
        c1 = juce::Colour::fromString(e.getStringAttribute("c1", "ffff2020"));
        c2 = juce::Colour::fromString(e.getStringAttribute("c2", "ffff8a00"));
        c3 = juce::Colour::fromString(e.getStringAttribute("c3", "ff550000"));
        rainbow = e.getBoolAttribute("rainbow", false);
        speed = (float)e.getDoubleAttribute("speed", 1.0); intensity = (float)e.getDoubleAttribute("intensity", 0.8);
        scale = (float)e.getDoubleAttribute("scale", 0.5); density = (float)e.getDoubleAttribute("density", 0.5);
        opacity = (float)e.getDoubleAttribute("opacity", 1.0); react = (float)e.getDoubleAttribute("react", 0.6);
        text = e.getStringAttribute("text"); path = e.getStringAttribute("path");
    }
};

struct SkinData
{
    SkinLayer layers[kNumLayers];
    juce::Colour accent{ 0xffff2a2a }, accent2{ 0xffff8a00 }, panel{ 0xff0a0204 }, text{ 0xffffe8e8 };
    float panelAlpha = 0.55f;
    int fps = 30;
    float renderScale = 1.f;
    juce::String name{ "Red Matrix" };
    juce::String videoPath; bool videoOn = false;
    int w = 1100, h = 780;

    SkinData()
    {
        auto& l = layers[0];
        l.enabled = true; l.type = VfxMatrix; l.variant = 0;
        l.c1 = juce::Colour(0xffff3030); l.c2 = juce::Colour(0xffd01010); l.c3 = juce::Colour(0xff400000);
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto e = std::make_unique<juce::XmlElement>("MvxSkin");
        e->setAttribute("name", name);
        e->setAttribute("accent", accent.toString()); e->setAttribute("accent2", accent2.toString());
        e->setAttribute("panel", panel.toString()); e->setAttribute("text", text.toString());
        e->setAttribute("panelAlpha", (double)panelAlpha); e->setAttribute("fps", fps);
        e->setAttribute("renderScale", (double)renderScale);
        e->setAttribute("videoPath", videoPath); e->setAttribute("videoOn", videoOn);
        e->setAttribute("w", w); e->setAttribute("h", h);
        for (auto& l : layers) e->addChildElement(l.toXml().release());
        return e;
    }
    void fromXml(const juce::XmlElement& e)
    {
        name = e.getStringAttribute("name", "Custom");
        accent = juce::Colour::fromString(e.getStringAttribute("accent", "ffff2a2a"));
        accent2 = juce::Colour::fromString(e.getStringAttribute("accent2", "ffff8a00"));
        panel = juce::Colour::fromString(e.getStringAttribute("panel", "ff0a0204"));
        text = juce::Colour::fromString(e.getStringAttribute("text", "ffffe8e8"));
        panelAlpha = (float)e.getDoubleAttribute("panelAlpha", 0.55);
        fps = juce::jlimit(10, 60, e.getIntAttribute("fps", 30));
        renderScale = juce::jlimit(0.3f, 1.f, (float)e.getDoubleAttribute("renderScale", 1.0));
        videoPath = e.getStringAttribute("videoPath"); videoOn = e.getBoolAttribute("videoOn", false);
        w = juce::jlimit(300, 6000, e.getIntAttribute("w", 1100)); h = juce::jlimit(250, 4000, e.getIntAttribute("h", 780));
        int i = 0;
        for (auto* c : e.getChildWithTagNameIterator("Layer")) { if (i >= kNumLayers) break; layers[i++].fromXml(*c); }
        for (; i < kNumLayers; ++i) layers[i] = SkinLayer();
    }
};

// ----------------------------------------------------------------------------
//  Documents\Hazel Bandz\MultiverseX\  : Media, Skins, settings.xml
// ----------------------------------------------------------------------------
inline juce::File mvxRoot()
{
    auto d = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                 .getChildFile("Hazel Bandz").getChildFile("MultiverseX");
    d.createDirectory();
    return d;
}
inline juce::File mvxMediaDir() { auto d = mvxRoot().getChildFile("Media"); d.createDirectory(); return d; }
inline juce::File mvxSkinsDir() { auto d = mvxRoot().getChildFile("Skins"); d.createDirectory(); return d; }
inline juce::File mvxSettingsFile() { return mvxRoot().getChildFile("settings.xml"); }

inline juce::String mvxGetSetting(const juce::String& key, const juce::String& def = {})
{
    if (auto x = juce::parseXML(mvxSettingsFile())) return x->getStringAttribute(key, def);
    return def;
}
inline void mvxSetSetting(const juce::String& key, const juce::String& value)
{
    auto x = juce::parseXML(mvxSettingsFile());
    if (x == nullptr) x = std::make_unique<juce::XmlElement>("Settings");
    x->setAttribute(key, value);
    x->writeTo(mvxSettingsFile());
}

// Finds a remembered file even if the original moved (looks in the Media folder by name).
inline juce::File mvxResolve(const juce::String& path)
{
    if (path.isEmpty()) return {};
    juce::File f(path);
    if (f.existsAsFile()) return f;
    auto alt = mvxMediaDir().getChildFile(f.getFileName());
    return alt.existsAsFile() ? alt : juce::File();
}

// Copies small media into the library so the skin keeps working if the original is moved.
inline juce::String mvxImport(const juce::File& src, bool copyIntoLibrary)
{
    if (!copyIntoLibrary || src.getSize() > 300LL * 1024 * 1024) return src.getFullPathName();
    auto dest = mvxMediaDir().getChildFile(src.getFileName());
    if (dest != src && (!dest.existsAsFile() || dest.getSize() != src.getSize())) src.copyFileTo(dest);
    return dest.existsAsFile() ? dest.getFullPathName() : src.getFullPathName();
}

// ----------------------------------------------------------------------------
//  Factory skins
// ----------------------------------------------------------------------------
inline SkinLayer mkLayer(int type, int variant, juce::uint32 a, juce::uint32 b, juce::uint32 c,
                         float speed = 1.f, float inten = 0.85f, float scale = 0.5f, float dens = 0.5f,
                         float opacity = 1.f, float react = 0.6f, bool rainbow = false, const char* text = "")
{
    SkinLayer l; l.enabled = true; l.type = type; l.variant = variant;
    l.c1 = juce::Colour(a); l.c2 = juce::Colour(b); l.c3 = juce::Colour(c);
    l.speed = speed; l.intensity = inten; l.scale = scale; l.density = dens; l.opacity = opacity; l.react = react;
    l.rainbow = rainbow; l.text = text;
    return l;
}

inline const char* const kSkinPresetNames[] = {
    "Red Matrix (default)", "Green Matrix", "Rainbow Matrix", "Hellfire", "Pyro Night", "Thunderstorm",
    "Electric Plasma", "Blue Water Ripples", "Rain Pool", "Tropical Sunset", "Tropical Night",
    "Wisdom Oak", "Ginkgo Gold", "Willow Moon", "Cedar Forest", "Chicago Studio Orbit", "Dub Edition Chrome",
    "Dub Edition Trunk Subs", "Keysight Rainbow", "Keysight Teal", "Keysight 3D Glow", "Red Aurora", "Red Warp Speed"
};
inline constexpr int kNumSkinPresets = (int)(sizeof(kSkinPresetNames) / sizeof(kSkinPresetNames[0]));

inline SkinData makeFactorySkin(int i)
{
    SkinData s; s.name = kSkinPresetNames[juce::jlimit(0, kNumSkinPresets - 1, i)];
    for (auto& l : s.layers) l = SkinLayer();
    auto theme = [&](juce::uint32 acc, juce::uint32 acc2, juce::uint32 pan) {
        s.accent = juce::Colour(acc); s.accent2 = juce::Colour(acc2); s.panel = juce::Colour(pan);
    };
    switch (i) {
    default:
    case 0: theme(0xffff2a2a, 0xffff8a00, 0xff0a0204);
            s.layers[0] = mkLayer(VfxMatrix, 0, 0xffff3030, 0xffd01010, 0xff400000); break;
    case 1: theme(0xff22ff55, 0xff9affb0, 0xff020a04);
            s.layers[0] = mkLayer(VfxMatrix, 0, 0xff30ff60, 0xff10b030, 0xff044010); break;
    case 2: theme(0xffff3df2, 0xff3dd9ff, 0xff08020c);
            s.layers[0] = mkLayer(VfxMatrix, 4, 0xffffffff, 0xffff00ff, 0xff0000ff, 1.1f, 0.9f, 0.5f, 0.7f, 1.f, 0.6f, true); break;
    case 3: theme(0xffff5a10, 0xffffd060, 0xff0c0402);
            s.layers[0] = mkLayer(VfxMatrix, 0, 0xffff6020, 0xff901000, 0xff300000, 0.7f, 0.35f, 0.4f, 0.4f);
            s.layers[1] = mkLayer(VfxFire, 0, 0xffffe070, 0xffff4a00, 0xff200000, 1.f, 0.9f, 0.6f, 0.7f, 1.f, 0.8f); break;
    case 4: theme(0xffffc83d, 0xffff3d6a, 0xff04040c);
            s.layers[0] = mkLayer(VfxStarfield, 1, 0xffffffff, 0xff9ab0ff, 0xff304080, 0.3f, 0.6f, 0.4f, 0.5f);
            s.layers[1] = mkLayer(VfxPyro, 0, 0xffffd060, 0xffff3040, 0xff3060ff, 1.f, 0.9f, 0.5f, 0.6f, 1.f, 0.7f); break;
    case 5: theme(0xff9ac8ff, 0xffffffff, 0xff04060c);
            s.layers[0] = mkLayer(VfxLightning, 0, 0xffa8d0ff, 0xff4060ff, 0xff151a30, 1.f, 0.9f, 0.6f, 0.5f, 1.f, 0.8f); break;
    case 6: theme(0xffb060ff, 0xff40e0ff, 0xff06030c);
            s.layers[0] = mkLayer(VfxLightning, 2, 0xffe0a0ff, 0xff7030ff, 0xff200840, 1.f, 0.95f, 0.6f, 0.8f, 1.f, 0.9f); break;
    case 7: theme(0xff3aa0ff, 0xff9ae0ff, 0xff020812);
            s.layers[0] = mkLayer(VfxWater, 0, 0xffa8d8ff, 0xff2060c0, 0xff05122a, 1.f, 0.9f, 0.55f, 0.5f); break;
    case 8: theme(0xff6ab8ff, 0xffc0e8ff, 0xff03060e);
            s.layers[0] = mkLayer(VfxWater, 2, 0xffb0e0ff, 0xff3070a0, 0xff081828, 1.f, 0.9f, 0.4f, 0.8f); break;
    case 9: theme(0xffff8a3a, 0xffffd070, 0xff12060a);
            s.layers[0] = mkLayer(VfxTropical, 1, 0xff5a2a8a, 0xffff9a40, 0xff0a6a8a, 1.f, 0.9f, 0.5f, 0.6f, 1.f, 0.5f); break;
    case 10: theme(0xff3adfff, 0xff9ab0ff, 0xff020812);
            s.layers[0] = mkLayer(VfxTropical, 2, 0xff050a28, 0xff1a4a7a, 0xff053048, 0.8f, 0.9f, 0.5f, 0.6f, 1.f, 0.5f); break;
    case 11: theme(0xff6ee06a, 0xffd0ff80, 0xff040a04);
            s.layers[0] = mkLayer(VfxWisdomTree, 2, 0xff3fa83a, 0xffc8ff70, 0xff4a2c14, 1.f, 0.9f, 0.5f, 0.0f, 1.f, 0.5f); break;
    case 12: theme(0xffffd23a, 0xffffffff, 0xff0c0a02);
            s.layers[0] = mkLayer(VfxWisdomTree, 3, 0xffffc820, 0xfffff0a0, 0xff3a2814, 1.f, 0.95f, 0.55f, 0.0f, 1.f, 0.5f); break;
    case 13: theme(0xff8ab8ff, 0xffd0e0ff, 0xff02040c);
            s.layers[0] = mkLayer(VfxStarfield, 1, 0xffffffff, 0xffa0c0ff, 0xff304070, 0.15f, 0.6f, 0.4f, 0.5f);
            s.layers[1] = mkLayer(VfxWisdomTree, 6, 0xff7aa8d0, 0xffd8eaff, 0xff2a2a3a, 0.8f, 0.9f, 0.55f, 0.0f); break;
    case 14: theme(0xff5ac86a, 0xffa8ff9a, 0xff02080a);
            s.layers[0] = mkLayer(VfxWisdomTree, 1, 0xff2f8a50, 0xff90ffb0, 0xff3a2a1a, 1.f, 0.9f, 0.45f, 0.8f); break;
    case 15: theme(0xffff2a2a, 0xffffd060, 0xff08060a);
            s.layers[0] = mkLayer(VfxStudio, 0, 0xffff2a3a, 0xffffb030, 0xff6a60c0, 1.f, 0.9f, 0.5f, 0.5f, 1.f, 0.8f, false, "CHICAGO BEATS"); break;
    case 16: theme(0xff40d8ff, 0xffff40c0, 0xff04060c);
            s.layers[0] = mkLayer(VfxCar, 0, 0xff1a2a3a, 0xff40d8ff, 0xffffffd0, 1.f, 0.9f, 0.7f, 0.5f, 1.f, 0.7f, false, "HAZEL BANDZ");
            s.layers[1] = mkLayer(VfxStarfield, 1, 0xffffffff, 0xff80a0ff, 0xff202040, 0.2f, 0.4f, 0.3f, 0.3f, 0.6f); break;
    case 17: theme(0xffff30d0, 0xffff9a40, 0xff0a0210);
            s.layers[0] = mkLayer(VfxCar, 2, 0xff301040, 0xffff30d0, 0xffffa040, 1.f, 0.9f, 0.5f, 0.5f, 1.f, 1.0f, false, "CHICAGO BEATS"); break;
    case 18: theme(0xffff4aa0, 0xff4ad0ff, 0xff04020c);
            s.layers[0] = mkLayer(VfxNeonNotes, 0, 0xffff3040, 0xff40ff60, 0xff3060ff, 1.f, 0.9f, 0.5f, 0.6f, 1.f, 0.8f, true); break;
    case 19: theme(0xff2fe8d0, 0xff9afff0, 0xff020a0c);
            s.layers[0] = mkLayer(VfxNeonNotes, 2, 0xff40ffe8, 0xff2090b0, 0xff0a2a40, 1.f, 0.9f, 0.5f, 0.6f, 1.f, 0.8f); break;
    case 20: theme(0xffb060ff, 0xff60ff90, 0xff06030c);
            s.layers[0] = mkLayer(VfxNeonNotes, 1, 0xffff40c0, 0xff40d0ff, 0xff60ff60, 1.f, 0.95f, 0.5f, 0.7f, 1.f, 0.8f, true); break;
    case 21: theme(0xffff3050, 0xffff9a60, 0xff0a0208);
            s.layers[0] = mkLayer(VfxAurora, 3, 0xffff3050, 0xffff8030, 0xff600030, 1.f, 0.85f, 0.5f, 0.5f, 1.f, 0.6f); break;
    case 22: theme(0xffff2a2a, 0xffffa060, 0xff0a0204);
            s.layers[0] = mkLayer(VfxStarfield, 0, 0xffff4040, 0xffffa060, 0xff600000, 1.2f, 0.9f, 0.5f, 0.7f, 1.f, 0.8f); break;
    }
    return s;
}

inline SkinData makeRandomSkin()
{
    juce::Random r;
    SkinData s = makeFactorySkin(0);
    s.name = "Random " + juce::String(r.nextInt(9999));
    const float hue = r.nextFloat();
    auto col = [&](float h, float sat, float v) { return juce::Colour::fromHSV(std::fmod(h + 1.f, 1.f), sat, v, 1.f); };
    s.accent = col(hue, 0.85f, 1.f); s.accent2 = col(hue + 0.12f, 0.8f, 1.f); s.panel = col(hue, 0.6f, 0.06f);
    static const int pool[] = { VfxMatrix, VfxFire, VfxPyro, VfxLightning, VfxWater, VfxTropical, VfxWisdomTree,
                                VfxNeonNotes, VfxStudio, VfxCar, VfxAurora, VfxStarfield, VfxGalaxy };
    const int nLayers = 1 + r.nextInt(3);
    for (auto& l : s.layers) l = SkinLayer();
    for (int i = 0; i < nLayers; ++i) {
        auto& l = s.layers[i];
        l.enabled = true; l.type = pool[r.nextInt((int)(sizeof(pool) / sizeof(pool[0])))];
        l.variant = r.nextInt(12);
        const float h = hue + (i == 0 ? 0.f : r.nextFloat() * 0.3f);
        l.c1 = col(h, 0.8f, 1.f); l.c2 = col(h + 0.1f, 0.9f, 0.9f); l.c3 = col(h + 0.05f, 0.8f, 0.3f);
        l.rainbow = r.nextInt(6) == 0;
        l.speed = 0.6f + r.nextFloat() * 1.0f; l.intensity = 0.7f + r.nextFloat() * 0.3f;
        l.scale = 0.3f + r.nextFloat() * 0.5f; l.density = 0.3f + r.nextFloat() * 0.6f;
        l.opacity = i == 0 ? 1.f : 0.55f + r.nextFloat() * 0.4f; l.react = 0.5f + r.nextFloat() * 0.4f;
        if (l.type == VfxStudio || l.type == VfxCar) l.text = "CHICAGO BEATS";
    }
    return s;
}
