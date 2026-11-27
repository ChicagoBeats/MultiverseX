#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include "Skin.h"
#include "GifLoader.h"

// Live info the effects react to
struct VfxContext
{
    float dt = 0.033f, time = 0.f, level = 0.f, lfo1 = 0.f, lfo2 = 0.f;
    float noteVel[128] = {};
};

// One renderer per skin layer (keeps that layer's particles / state).
class VfxRenderer
{
public:
    void paint(juce::Graphics& g, juce::Rectangle<float> area, const SkinLayer& layer, const VfxContext& ctx);

private:
    struct P { float x = 0, y = 0, vx = 0, vy = 0, life = 0, max = 1, size = 1, u = 0, aux = 0; int id = 0, kind = 0; };
    struct Seg { float x1, y1, x2, y2, w; };
    struct Bolt { std::vector<Seg> segs; float life = 0, max = 1; juce::Colour col; };
    struct TreeSpec { float spread, ratio; int branches; int depth; float thick, leaf, droop, jitter; int kind; };
    struct Cam { float px, py, pz, rx, ry, rz, ux, uy, uz, fx, fy, fz, focal, cx, cy; };

    // helpers
    juce::Colour pick(float u) const;
    void reset();
    void blob(juce::Graphics& g, float cx, float cy, float r, juce::Colour c) const;

    // effects
    void paintGalaxy(juce::Graphics&);   void paintMatrix(juce::Graphics&);  void paintFire(juce::Graphics&);
    void paintPyro(juce::Graphics&);     void paintLightning(juce::Graphics&); void paintWater(juce::Graphics&);
    void paintTropical(juce::Graphics&); void paintTrees(juce::Graphics&);   void paintNeon(juce::Graphics&);
    void paintStudio(juce::Graphics&);   void paintCar(juce::Graphics&);     void paintAurora(juce::Graphics&);
    void paintStarfield(juce::Graphics&); void paintMedia(juce::Graphics&);  void paintGradient(juce::Graphics&);

    // sub-helpers
    void ensureAtlas(int var, float fontPx, int cell);
    void genBolt(std::vector<Seg>& out, float x1, float y1, float x2, float y2, float disp, int depth, float w);
    void drawPalm(juce::Graphics& g, float bx, float by, float h, float lean, float phase, float wind, juce::Colour leaf, juce::Colour trunk, bool detailed) const;
    void treeBranch(juce::Graphics& g, float x, float y, float ang, float len, float thick, int depth, int maxDepth,
                    const TreeSpec& s, float sway, uint32_t seed, float alpha, juce::Colour trunk, int species);
    void drawConifer(juce::Graphics& g, float x, float y, float size, bool cedar, float alpha, juce::Colour trunk);
    void drawWheel(juce::Graphics& g, float cx, float cy, float r, int style, float rot) const;
    void drawWoofer(juce::Graphics& g, float cx, float cy, float r, float pulse, juce::Colour ring, juce::Colour cone) const;
    void paintCarSide(juce::Graphics&);  void paintCarInterior(juce::Graphics&);  void paintCarTrunk(juce::Graphics&);
    bool proj(const Cam& c, float x, float y, float z, float& sx, float& sy) const;
    void line3(juce::Graphics& g, const Cam& c, float x1, float y1, float z1, float x2, float y2, float z2, juce::Colour col, float w) const;
    void box3(juce::Graphics& g, const Cam& c, float x0, float y0, float z0, float x1, float y1, float z1, juce::Colour col, float w) const;
    void ring3(juce::Graphics& g, const Cam& c, float cx, float cy, float cz, float r, juce::Colour col, float w) const;
    void layoutKeys(float W);
    juce::Colour noteColour(int n) const;

    // per-paint context
    const SkinLayer* L_ = nullptr; const VfxContext* c_ = nullptr;
    float W = 100, H = 100, dt_ = 0.03f, lvl_ = 0.f, t = 0.f;
    int var_ = 0;

    // generic state
    int lastType = -1, lastVariant = -1; float lastW = 0, lastH = 0;
    juce::Random rng;
    std::vector<P> ps;
    float tm[8] = {};
    std::vector<int> newNotes;
    float prevVel[128] = {}, curVel[128] = {};

    // galaxy
    float gRot = 0, gSc = 1, gx = 0, gy = 0, gvx = 0, gvy = 0;
    // matrix
    juce::Image atlas; int atlasCell = 0, atlasVar = -1, atlasN = 1; float atlasPx = 0;
    // lightning / pyro
    std::vector<Bolt> bolts; float flash = 0;
    // neon notes
    float keyX[128] = {}, keyGlow[128] = {}, keyW = 10, keyLayoutW = 0; bool keyBlack[128] = {};
    // media
    juce::Image img; GifAnim gif; juce::String loadedPath; int gifIdx = 0; float gifAcc = 0;
    std::unique_ptr<juce::Drawable> svg; bool mediaMissing = false;
};
