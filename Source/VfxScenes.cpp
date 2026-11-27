#include "Vfx.h"
#include "VfxUtil.h"

using juce::Colour;
using juce::Graphics;

// ============================================================================
//  Tropical island: moving palms, waves, sun / moon, jungle swaying
// ============================================================================
void VfxRenderer::drawPalm(Graphics& g, float bx, float by, float h, float lean, float phase, float wind,
                           Colour leaf, Colour trunk, bool detailed) const
{
    const int seg = 10;
    float px = bx, py = by;
    for (int i = 1; i <= seg; ++i) {
        const float f = (float)i / (float)seg;
        const float x = bx + lean * h * f * f + std::sin(t * 0.9f + phase) * wind * h * 0.10f * f * f;
        const float y = by - h * f;
        g.setColour(trunk);
        g.drawLine(px, py, x, y, juce::jmax(1.5f, h * 0.05f * (1.f - 0.5f * f)));
        px = x; py = y;
    }
    const float cx = px, cy = py;
    if (detailed) {
        g.setColour(Colour(0xff3a2410));
        for (int k = 0; k < 3; ++k) g.fillEllipse(cx - h * 0.03f + (float)(k - 1) * h * 0.03f, cy + h * 0.01f, h * 0.035f, h * 0.035f);
    }
    for (int k = 0; k < 9; ++k) {
        const float a = ((float)k - 4.f) / 4.f * 1.75f;
        const float dx = std::sin(a), dy = -std::cos(a);
        const float len = h * 0.55f * (0.85f + 0.3f * hash01((uint32_t)(k * 13 + (int)(phase * 10.f))));
        const float sw = std::sin(t * 1.4f + phase + (float)k) * wind * len * 0.18f;
        const juce::Point<float> c0(cx, cy), ctrl(cx + dx * len * 0.6f, cy + dy * len * 0.6f - len * 0.35f),
                                 en(cx + dx * len * 0.95f + sw, cy + dy * len * 0.7f + len * 0.3f);
        juce::Path p; p.startNewSubPath(c0); p.quadraticTo(ctrl, en);
        const Colour lc = (k % 2) ? leaf.brighter(0.2f) : leaf;
        g.setColour(lc);
        g.strokePath(p, juce::PathStrokeType(juce::jmax(1.5f, h * 0.03f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (!detailed) continue;
        for (int j = 1; j <= 10; ++j) {
            const float s = (float)j / 11.f, is = 1.f - s;
            const float bxp = is * is * c0.x + 2.f * is * s * ctrl.x + s * s * en.x;
            const float byp = is * is * c0.y + 2.f * is * s * ctrl.y + s * s * en.y;
            float tx = 2.f * is * (ctrl.x - c0.x) + 2.f * s * (en.x - ctrl.x);
            float ty = 2.f * is * (ctrl.y - c0.y) + 2.f * s * (en.y - ctrl.y);
            const float tl = std::sqrt(tx * tx + ty * ty) + 0.0001f; tx /= tl; ty /= tl;
            const float ll = len * 0.2f * (1.f - s * 0.6f);
            for (int side = -1; side <= 1; side += 2) {
                g.drawLine(bxp, byp, bxp + (-ty * (float)side) * ll + tx * ll * 0.5f,
                           byp + (tx * (float)side) * ll + ty * ll * 0.5f + ll * 0.3f, 1.4f);
            }
        }
    }
}

void VfxRenderer::paintTropical(Graphics& g)
{
    const int v = var_ % 4;
    const float I = L_->intensity, yH = H * 0.5f;
    const float wind = 0.6f + lvl_ * 1.2f;

    // sky
    g.setGradientFill(juce::ColourGradient(L_->c1, 0.f, 0.f, L_->c2, 0.f, yH, false));
    g.fillRect(0.f, 0.f, W, yH + 1.f);
    if (v == 2)
        for (int i = 0; i < 80; ++i) {
            g.setColour(juce::Colours::white.withAlpha(0.3f + 0.6f * std::abs(std::sin(t * 1.5f + (float)i))));
            g.fillRect(hash01((uint32_t)(i * 3 + 1)) * W, hash01((uint32_t)(i * 7 + 2)) * yH * 0.8f, 1.8f, 1.8f);
        }

    // sun / moon
    const float sunX = W * 0.72f, sunY = (v == 1 || v == 3) ? yH - H * 0.06f : yH - H * 0.28f;
    if (v == 2) {
        blob(g, sunX, sunY, H * 0.18f, juce::Colours::white.withAlpha(0.3f * I));
        g.setColour(Colour(0xffeef2ff)); g.fillEllipse(sunX - H * 0.05f, sunY - H * 0.05f, H * 0.1f, H * 0.1f);
        g.setColour(L_->c1); g.fillEllipse(sunX - H * 0.035f, sunY - H * 0.06f, H * 0.09f, H * 0.09f);
    } else {
        blob(g, sunX, sunY, H * 0.3f, L_->c2.brighter(0.4f).withAlpha(0.5f * I));
        g.setColour(Colour(0xffffe9a0)); g.fillEllipse(sunX - H * 0.06f, sunY - H * 0.06f, H * 0.12f, H * 0.12f);
    }
    // clouds
    for (int i = 0; i < 5; ++i) {
        const float x = std::fmod(hash01((uint32_t)(i * 5 + 1)) * W + t * 8.f * (0.5f + hash01((uint32_t)(i * 9))), W + 300.f) - 150.f;
        blob(g, x, yH * (0.2f + 0.5f * hash01((uint32_t)(i * 11 + 3))), 80.f + 40.f * hash01((uint32_t)i),
             (v == 2 ? L_->c2 : juce::Colours::white).withAlpha(0.22f));
    }

    Colour leaf = Colour::fromHSV(std::fmod(0.33f + (L_->rainbow ? t * 0.05f : 0.f), 1.f), 0.65f, v == 2 ? 0.38f : 0.62f, 1.f)
                      .interpolatedWith(L_->c2, 0.15f);
    const Colour trunk(v == 2 ? 0xff2a1c10 : 0xff6a4a2a);

    // far island + moving jungle
    juce::Path hills; hills.startNewSubPath(W * 0.5f, yH);
    for (float x = W * 0.5f; x <= W; x += 12.f)
        hills.lineTo(x, yH - H * 0.07f * std::sin(kPi * (x - W * 0.5f) / (W * 0.5f)));
    hills.lineTo(W, yH); hills.closeSubPath();
    g.setColour(L_->c3.darker(0.8f).withAlpha(0.9f)); g.fillPath(hills);
    const int nJungle = 14 + (int)(L_->density * 30.f);
    for (int i = 0; i < nJungle; ++i) {
        const float x = W * 0.5f + W * 0.5f * ((float)i + 0.5f) / (float)nJungle;
        const float ridge = yH - H * 0.07f * std::sin(kPi * (x - W * 0.5f) / (W * 0.5f));
        const float hh = hash01((uint32_t)(i * 7 + 9));
        drawPalm(g, x, ridge + 2.f, H * (0.05f + 0.04f * hh), (hh - 0.5f) * 0.4f, hh * 6.f, wind, leaf.darker(0.5f), trunk.darker(0.5f), false);
    }

    // sea
    g.setGradientFill(juce::ColourGradient(L_->c3.brighter(0.2f), 0.f, yH, L_->c3.darker(0.7f), 0.f, H, false));
    g.fillRect(0.f, yH, W, H - yH);
    // sun glitter
    for (int j = 0; j < 40; ++j) {
        const float y = yH + (float)j * (H - yH) / 45.f;
        const float w = (6.f + (float)j * 2.f) * (0.3f + hash01((uint32_t)(j * 3 + (int)(t * 4.f))));
        g.setColour(L_->c2.brighter(0.6f).withAlpha(0.35f * I * (1.f - (float)j / 45.f)));
        g.fillRect(sunX - w * 0.5f + std::sin(t * 2.f + (float)j) * 4.f, y, w, 2.f);
    }
    for (int k = 0; k < 6; ++k) {
        const float y0 = yH + (H - yH) * std::pow((float)k / 6.f, 1.3f) + 4.f;
        const float amp = 3.f + (float)k * 3.5f * (0.5f + lvl_), wl = 90.f + (float)k * 70.f;
        juce::Path fillP, crest;
        fillP.startNewSubPath(0.f, H);
        for (float x = 0.f; x <= W + 8.f; x += 8.f) {
            const float y = y0 + std::sin(x / wl * kTwoPi + t * (0.6f + (float)k * 0.15f) + (float)k) * amp;
            fillP.lineTo(x, y);
            if (x == 0.f) crest.startNewSubPath(x, y); else crest.lineTo(x, y);
        }
        fillP.lineTo(W, H); fillP.closeSubPath();
        g.setColour(L_->c3.interpolatedWith(L_->c1, 0.15f + 0.08f * (float)k).withAlpha(0.3f * I));
        g.fillPath(fillP);
        if (k >= 3) { g.setColour(juce::Colours::white.withAlpha(0.22f)); g.strokePath(crest, juce::PathStrokeType(1.6f)); }
    }

    // beach + palms
    juce::Path sand;
    sand.startNewSubPath(0.f, H); sand.lineTo(0.f, H * 0.80f);
    sand.cubicTo(W * 0.18f, H * 0.74f, W * 0.42f, H * 0.80f, W * 0.62f, H * 0.92f);
    sand.lineTo(W * 0.7f, H); sand.closeSubPath();
    g.setColour(Colour(0xffd9b56e).interpolatedWith(L_->c2, 0.25f).darker(v == 2 ? 0.6f : 0.f));
    g.fillPath(sand);
    const int nPalms = 3 + (int)(L_->density * 7.f);
    for (int i = 0; i < nPalms; ++i) {
        const float hh = hash01((uint32_t)(i * 3 + 17));
        const float xn = 0.04f + 0.5f * ((float)i + hh) / (float)nPalms;
        const float by = H * (0.79f + 0.1f * std::pow(xn / 0.62f, 2.f)) + 6.f;
        drawPalm(g, W * xn, by, H * (0.28f + 0.18f * hh) * (0.7f + L_->scale * 0.6f), (hh - 0.5f) * 0.3f, (float)i * 1.3f, wind, leaf, trunk, true);
    }
}

// ============================================================================
//  Wisdom trees (pine, cedar, oak, ginkgo, fig, beech, willow) + forest mode
// ============================================================================
void VfxRenderer::treeBranch(Graphics& g, float x, float y, float ang, float len, float thick, int depth, int maxDepth,
                             const TreeSpec& s, float sway, uint32_t seed, float alpha, Colour trunk, int species)
{
    const float k = (float)(maxDepth - depth) / (float)juce::jmax(1, maxDepth);
    const float a = ang + std::sin(t * 1.1f + (float)seed * 0.013f + (float)depth * 0.9f) * sway * (0.2f + k);
    const float x2 = x + std::sin(a) * len, y2 = y - std::cos(a) * len;
    g.setColour(trunk.withAlpha(alpha));
    g.drawLine(x, y, x2, y2, thick);

    if (depth <= 0) {
        const float h1 = hash01(seed * 7u + 1u);
        const float ls = s.leaf * (len / 20.f + 0.6f) * (0.7f + h1);
        const Colour lc = pick(h1 * 0.6f);
        g.setColour(lc.withAlpha(juce::jlimit(0.f, 1.f, 0.65f * L_->intensity * alpha)));
        g.fillEllipse(x2 - ls, y2 - ls, ls * 2.f, ls * 1.6f);
        if ((seed % 6u) == 0u) blob(g, x2, y2, ls * 3.f, pick(0.6f).withAlpha(0.18f * alpha));
        if (species == 6) {
            g.setColour(lc.withAlpha(0.5f * alpha));
            g.drawLine(x2, y2, x2 + std::sin(t + (float)seed) * 6.f, y2 + len * 3.f + 14.f, 1.2f);
        }
        return;
    }
    if (species == 4 && depth == 1) {
        g.setColour(trunk.withAlpha(0.6f * alpha));
        g.drawLine(x2, y2, x2 + std::sin(t * 0.8f + (float)seed) * 3.f, y2 + len * (2.f + hash01(seed)), 1.5f);
    }
    for (int i = 0; i < s.branches; ++i) {
        const float off = (float)i - (float)(s.branches - 1) * 0.5f;
        const float j = (hash01(seed * 31u + (uint32_t)(depth * 7 + i)) - 0.5f) * s.jitter;
        float na = a + off * s.spread * (1.f + j);
        if (s.droop > 0.f) na += s.droop * (a >= 0.f ? 1.f : -1.f) * (0.3f + k);
        na = juce::jlimit(-2.9f, 2.9f, na);
        treeBranch(g, x2, y2, na, len * s.ratio * (0.9f + j * 0.4f), juce::jmax(0.8f, thick * 0.7f), depth - 1, maxDepth, s,
                   sway, seed * 3u + (uint32_t)i + 1u, alpha, trunk, species);
    }
}

void VfxRenderer::drawConifer(Graphics& g, float x, float y, float size, bool cedar, float alpha, Colour trunk)
{
    const float trunkH = size * 3.2f;
    g.setColour(trunk.withAlpha(alpha));
    g.drawLine(x, y, x, y - trunkH, size * 0.1f);
    const int tiers = 11;
    const float sway = 0.04f + lvl_ * 0.06f;
    for (int ti = 0; ti < tiers; ++ti) {
        const float f = (float)ti / (float)(tiers - 1);
        const float ty = y - trunkH * (0.12f + 0.88f * f);
        const float wid = size * (1.6f * (1.f - f) + 0.25f) * (cedar ? 1.3f : 1.f);
        for (int side = -1; side <= 1; side += 2) {
            const float tipx = x + (float)side * wid;
            const float tipy = ty + wid * (cedar ? 0.12f : 0.55f) + std::sin(t * 1.2f + (float)ti + (float)side) * sway * wid;
            g.setColour(trunk.brighter(0.1f).withAlpha(alpha));
            g.drawLine(x, ty, tipx, tipy, juce::jmax(1.5f, size * 0.05f));
            g.setColour(pick(0.15f + f * 0.4f).withAlpha(juce::jlimit(0.f, 1.f, 0.8f * alpha * L_->intensity)));
            for (int m = 1; m < 9; ++m) {
                const float s = (float)m / 9.f, px = x + (tipx - x) * s, py = ty + (tipy - ty) * s;
                g.drawLine(px, py, px + (float)side * wid * 0.08f, py + wid * 0.14f, 1.3f);
            }
            if (cedar) g.fillEllipse(tipx - wid * 0.25f, tipy - wid * 0.05f, wid * 0.5f, wid * 0.14f);
        }
    }
    blob(g, x, y - trunkH, size * 0.8f, pick(0.6f).withAlpha(0.35f * alpha));
}

void VfxRenderer::paintTrees(Graphics& g)
{
    const int sp = var_ % 7;
    const float I = L_->intensity;
    static const TreeSpec specs[7] = {
        { 0.f, 0.f, 0, 0, 0.f, 0.f, 0.f, 0.f, 1 },                  // pine
        { 0.f, 0.f, 0, 0, 0.f, 0.f, 0.f, 0.f, 2 },                  // cedar
        { 0.55f, 0.72f, 3, 6, 16.f, 9.f, 0.f, 0.5f, 0 },            // oak
        { 0.40f, 0.80f, 2, 9, 14.f, 8.f, 0.f, 0.35f, 0 },           // ginkgo
        { 0.75f, 0.70f, 3, 6, 18.f, 10.f, 0.f, 0.5f, 3 },           // fig
        { 0.50f, 0.80f, 2, 9, 14.f, 8.f, 0.f, 0.3f, 0 },            // beech
        { 0.50f, 0.80f, 2, 8, 14.f, 7.f, 0.12f, 0.35f, 4 }          // willow
    };
    const TreeSpec& s = specs[sp];

    g.setGradientFill(juce::ColourGradient(L_->c3.darker(1.f).withAlpha(0.f), 0.f, H * 0.7f,
                                           L_->c3.darker(0.6f).withAlpha(0.9f * I), 0.f, H, false));
    g.fillRect(0.f, H * 0.7f, W, H * 0.3f);

    // wisdom fireflies
    for (auto& p : ps) {
        p.x += (p.vx + std::sin(t + p.aux) * 6.f) * dt_; p.y += (p.vy + std::cos(t * 0.7f + p.aux) * 5.f) * dt_;
        if (p.x < 0.f) p.x += W; if (p.x > W) p.x -= W; if (p.y < H * 0.1f) p.y = H * 0.9f; if (p.y > H) p.y = H * 0.15f;
        const float pulse = 0.5f + 0.5f * std::sin(t * 2.f + p.aux * 3.f);
        blob(g, p.x, p.y, p.size * (2.f + pulse * 2.f + lvl_ * 3.f), pick(0.5f + p.u * 0.5f).withAlpha(juce::jlimit(0.f, 1.f, (0.35f + pulse * 0.5f) * I)));
    }

    const int count = 1 + (int)(L_->density * 8.f);
    const float sway = 0.03f + lvl_ * 0.05f;
    for (int k = 0; k < count; ++k) {
        const float z = count == 1 ? 1.f : (float)(k + 1) / (float)count;
        const float x = count == 1 ? W * 0.5f : W * (0.08f + 0.84f * hash01((uint32_t)(k * 17 + 5)));
        const float size = H * (0.16f + L_->scale * 0.14f) * (0.45f + 0.55f * z);
        const float y = H * (0.80f + 0.17f * z);
        const float alpha = 0.45f + 0.55f * z;
        blob(g, x, y - size * 2.5f, size * 3.f, pick(0.55f).withAlpha(0.12f * I));
        if (s.kind == 1 || s.kind == 2) { drawConifer(g, x, y, size * 0.8f, s.kind == 2, alpha, L_->c3); continue; }
        int maxD = s.depth;
        const float b = (float)s.branches;
        while (maxD > 4 && (float)count * (std::pow(b, (float)maxD + 1.f) - 1.f) / (b - 1.f) > 4500.f) --maxD;
        const float sc = size / 150.f;
        treeBranch(g, x, y, 0.f, size * 0.7f, s.thick * 0.6f * sc * 1.6f, maxD, maxD, s, sway, (uint32_t)(k * 101 + 7), alpha, L_->c3, sp);
    }
}

// ============================================================================
//  Chicago Beats studio: wireframe 3D room, orbiting camera, skyline, branding
// ============================================================================
bool VfxRenderer::proj(const Cam& c, float x, float y, float z, float& sx, float& sy) const
{
    const float dx = x - c.px, dy = y - c.py, dz = z - c.pz;
    const float cz = dx * c.fx + dy * c.fy + dz * c.fz;
    if (cz < 0.2f) return false;
    const float cxp = dx * c.rx + dy * c.ry + dz * c.rz, cyp = dx * c.ux + dy * c.uy + dz * c.uz;
    sx = c.cx + cxp / cz * c.focal; sy = c.cy - cyp / cz * c.focal;
    return true;
}

void VfxRenderer::line3(Graphics& g, const Cam& c, float x1, float y1, float z1, float x2, float y2, float z2, Colour col, float w) const
{
    float ax, ay, bx, by;
    if (!proj(c, x1, y1, z1, ax, ay) || !proj(c, x2, y2, z2, bx, by)) return;
    g.setColour(col.withMultipliedAlpha(0.28f)); g.drawLine(ax, ay, bx, by, w * 3.5f);
    g.setColour(col); g.drawLine(ax, ay, bx, by, w);
}

void VfxRenderer::box3(Graphics& g, const Cam& c, float x0, float y0, float z0, float x1, float y1, float z1, Colour col, float w) const
{
    line3(g, c, x0, y0, z0, x1, y0, z0, col, w); line3(g, c, x0, y1, z0, x1, y1, z0, col, w);
    line3(g, c, x0, y0, z1, x1, y0, z1, col, w); line3(g, c, x0, y1, z1, x1, y1, z1, col, w);
    line3(g, c, x0, y0, z0, x0, y1, z0, col, w); line3(g, c, x1, y0, z0, x1, y1, z0, col, w);
    line3(g, c, x0, y0, z1, x0, y1, z1, col, w); line3(g, c, x1, y0, z1, x1, y1, z1, col, w);
    line3(g, c, x0, y0, z0, x0, y0, z1, col, w); line3(g, c, x1, y0, z0, x1, y0, z1, col, w);
    line3(g, c, x0, y1, z0, x0, y1, z1, col, w); line3(g, c, x1, y1, z0, x1, y1, z1, col, w);
}

void VfxRenderer::ring3(Graphics& g, const Cam& c, float cx, float cy, float cz, float r, Colour col, float w) const
{
    const int n = 20;
    for (int i = 0; i < n; ++i) {
        const float a0 = kTwoPi * (float)i / (float)n, a1 = kTwoPi * (float)(i + 1) / (float)n;
        line3(g, c, cx + std::cos(a0) * r, cy + std::sin(a0) * r, cz, cx + std::cos(a1) * r, cy + std::sin(a1) * r, cz, col, w);
    }
}

void VfxRenderer::paintStudio(Graphics& g)
{
    const int v = var_ % 3;
    const float I = L_->intensity, pulse = lvl_;
    float px, py, pz, tx, ty, tz;
    if (v == 0) { const float a = t * 0.3f; px = 9.5f * std::sin(a); py = 2.4f + std::sin(t * 0.2f) * 0.5f; pz = 9.5f * std::cos(a); tx = 0; ty = 1.2f; tz = -1.5f; }
    else if (v == 1) { const float a = std::sin(t * 0.2f) * 0.6f; px = std::sin(a) * 2.5f; py = 1.7f; pz = 2.f + std::cos(t * 0.15f) * 0.8f; tx = 0; ty = 1.2f; tz = -4.f; }
    else { const float a = t * 0.4f; px = 8.f * std::sin(a); py = 6.f + std::sin(t * 0.15f) * 2.5f; pz = 8.f * std::cos(a); tx = 0; ty = 0.5f; tz = -1.5f; }

    Cam c; c.px = px; c.py = py; c.pz = pz;
    float fx = tx - px, fy = ty - py, fz = tz - pz;
    float fl = std::sqrt(fx * fx + fy * fy + fz * fz); fx /= fl; fy /= fl; fz /= fl;
    float rx = -fz, ry = 0.f, rz = fx; const float rl = std::sqrt(rx * rx + rz * rz) + 0.0001f; rx /= rl; rz /= rl;
    c.fx = fx; c.fy = fy; c.fz = fz; c.rx = rx; c.ry = ry; c.rz = rz;
    c.ux = ry * fz - rz * fy; c.uy = rz * fx - rx * fz; c.uz = rx * fy - ry * fx;
    c.focal = juce::jmin(W, H) * 0.9f * (0.7f + L_->scale * 0.8f); c.cx = W * 0.5f; c.cy = H * 0.5f;

    const Colour c1 = L_->c1, c2 = L_->c2, c3 = L_->c3;
    // skyline (Chicago): Willis Tower + ring of towers
    for (int k = 0; k < 18; ++k) {
        const float ang = (float)k * kTwoPi / 18.f + 0.3f, r = 14.f + hash01((uint32_t)(k * 3 + 1)) * 5.f;
        const float bx = r * std::sin(ang), bz = r * std::cos(ang), w = 1.5f + hash01((uint32_t)(k * 5 + 2)) * 1.5f;
        const float h = 6.f + hash01((uint32_t)(k * 7 + 3)) * 18.f;
        box3(g, c, bx - w, 0.f, bz - w, bx + w, h, bz + w, c3.withAlpha(0.7f * I), 1.f);
    }
    box3(g, c, -11.f, 0.f, -15.f, -9.f, 18.f, -13.f, c3.withAlpha(0.9f * I), 1.2f);
    box3(g, c, -10.8f, 18.f, -14.8f, -9.2f, 26.f, -13.2f, c3.withAlpha(0.9f * I), 1.2f);
    box3(g, c, -10.5f, 26.f, -14.5f, -9.5f, 30.f, -13.5f, c3.withAlpha(0.9f * I), 1.2f);
    line3(g, c, -10.2f, 30.f, -14.f, -10.2f, 34.f, -14.f, c2.withAlpha(I), 1.2f);
    line3(g, c, -9.8f, 30.f, -14.f, -9.8f, 34.f, -14.f, c2.withAlpha(I), 1.2f);

    // floor grid + room
    for (int i = -6; i <= 6; ++i) {
        line3(g, c, (float)i, 0.f, -6.f, (float)i, 0.f, 6.f, c1.withAlpha(0.35f * I), 1.f);
        line3(g, c, -6.f, 0.f, (float)i, 6.f, 0.f, (float)i, c1.withAlpha(0.35f * I), 1.f);
    }
    box3(g, c, -6.f, 0.f, -6.f, 6.f, 4.f, 6.f, c1.withAlpha(0.9f * I), 1.6f);
    for (int k = -2; k <= 2; ++k)
        line3(g, c, -5.f, 3.9f, (float)k * 2.f, 5.f, 3.9f, (float)k * 2.f, c2.withAlpha(juce::jlimit(0.f, 1.f, (0.5f + pulse * 0.5f) * I)), 1.6f);

    // desk, console, monitors, speakers
    box3(g, c, -2.4f, 0.75f, -4.5f, 2.4f, 0.82f, -3.3f, c1.withAlpha(0.9f * I), 1.4f);
    for (int lx = 0; lx < 2; ++lx) for (int lz = 0; lz < 2; ++lz)
        line3(g, c, lx ? 2.3f : -2.3f, 0.f, lz ? -3.4f : -4.4f, lx ? 2.3f : -2.3f, 0.75f, lz ? -3.4f : -4.4f, c1.withAlpha(0.7f * I), 1.f);
    line3(g, c, -1.4f, 0.84f, -3.55f, 1.4f, 0.84f, -3.55f, c2.withAlpha(I), 1.4f);
    line3(g, c, -1.4f, 1.03f, -3.9f, 1.4f, 1.03f, -3.9f, c2.withAlpha(I), 1.4f);
    line3(g, c, -1.4f, 0.84f, -3.55f, -1.4f, 1.03f, -3.9f, c2.withAlpha(I), 1.4f);
    line3(g, c, 1.4f, 0.84f, -3.55f, 1.4f, 1.03f, -3.9f, c2.withAlpha(I), 1.4f);
    for (int i = 0; i < 16; ++i) {
        const float x = -1.3f + (float)i * 0.17f, s = 0.5f + 0.4f * std::sin(t * 2.f + (float)i) * (0.3f + pulse);
        line3(g, c, x, 0.84f, -3.55f, x, 1.03f, -3.9f, c1.withAlpha(0.4f * I), 1.f);
        line3(g, c, x - 0.04f, 0.84f + 0.19f * s, -3.55f - 0.35f * s, x + 0.04f, 0.84f + 0.19f * s, -3.55f - 0.35f * s, c2.withAlpha(I), 1.6f);
    }
    for (int m = 0; m < 2; ++m) {
        const float sx = m ? 0.9f : -0.9f;
        box3(g, c, sx - 0.45f, 0.9f, -4.4f, sx + 0.45f, 1.5f, -4.3f, c1.withAlpha(0.9f * I), 1.3f);
        for (int j = 0; j < 16; ++j) {
            const float xa = sx - 0.38f + 0.76f * (float)j / 16.f, xb = sx - 0.38f + 0.76f * (float)(j + 1) / 16.f;
            const float ya = 1.2f + std::sin(t * 6.f + (float)j * 0.8f + sx) * 0.22f * (0.3f + pulse);
            const float yb = 1.2f + std::sin(t * 6.f + (float)(j + 1) * 0.8f + sx) * 0.22f * (0.3f + pulse);
            line3(g, c, xa, ya, -4.29f, xb, yb, -4.29f, c2.withAlpha(I), 1.4f);
        }
    }
    for (int m = 0; m < 2; ++m) {
        const float sx = m ? 2.3f : -2.3f;
        box3(g, c, sx - 0.3f, 0.9f, -4.5f, sx + 0.3f, 1.9f, -3.9f, c1.withAlpha(0.9f * I), 1.3f);
        ring3(g, c, sx, 1.35f, -3.88f, 0.2f * (1.f + pulse * 0.25f), c2.withAlpha(I), 1.5f);
        ring3(g, c, sx, 1.7f, -3.88f, 0.06f, c2.withAlpha(I), 1.2f);
    }
    box3(g, c, 3.5f, 0.f, -2.f, 5.5f, 2.5f, 0.5f, c3.withAlpha(0.8f * I), 1.2f);

    // light beams
    for (int k = 0; k < 4; ++k) {
        const float lx = (k % 2) ? 3.f : -3.f, lz = (k < 2) ? -2.5f : 2.5f, a = t * 2.f + (float)k;
        float s0x, s0y, s1x, s1y, s2x, s2y;
        if (proj(c, lx, 3.9f, lz, s0x, s0y) && proj(c, lx + std::cos(a) * 1.4f, 0.f, lz + std::sin(a) * 1.4f, s1x, s1y)
            && proj(c, lx + std::cos(a + 0.7f) * 1.4f, 0.f, lz + std::sin(a + 0.7f) * 1.4f, s2x, s2y)) {
            juce::Path p; p.startNewSubPath(s0x, s0y); p.lineTo(s1x, s1y); p.lineTo(s2x, s2y); p.closeSubPath();
            g.setColour(pick((float)k / 4.f).withAlpha(0.12f * I)); g.fillPath(p);
        }
    }

    // brand sign on the back wall (Chicago Beats by default)
    {
        float s0x, s0y, s1x, s1y, s2x, s2y;
        if (proj(c, -3.f, 3.5f, -5.95f, s0x, s0y) && proj(c, 3.f, 3.5f, -5.95f, s1x, s1y) && proj(c, -3.f, 2.3f, -5.95f, s2x, s2y)
            && ((s1x - s0x) * (s2y - s0y) - (s1y - s0y) * (s2x - s0x)) > 0.f) {
            const float tw = 600.f, th = 120.f;
            g.saveState();
            g.addTransform(juce::AffineTransform::scale(1.f / tw, 1.f / th).followedBy(
                juce::AffineTransform::fromTargetPoints(s0x, s0y, s1x, s1y, s2x, s2y)));
            g.setFont(juce::Font("Arial Black", 84.f, juce::Font::bold));
            const juce::String txt = L_->text.isEmpty() ? juce::String("CHICAGO BEATS") : L_->text;
            const auto r = juce::Rectangle<int>(0, 0, (int)tw, (int)th);
            g.setColour(c2.withAlpha(0.35f * I)); g.drawText(txt, r.translated(3, 3), juce::Justification::centred, true);
            g.setColour(c1.withAlpha(juce::jlimit(0.f, 1.f, (0.8f + pulse * 0.2f) * I))); g.drawText(txt, r, juce::Justification::centred, true);
            g.restoreState();
        }
    }
}

// ============================================================================
//  Dub-style concept car: side view / interior with huge TV / trunk sub wall
// ============================================================================
void VfxRenderer::drawWoofer(Graphics& g, float cx, float cy, float r, float pulse, Colour ring, Colour cone) const
{
    g.setColour(ring.withAlpha(0.9f)); g.drawEllipse(cx - r, cy - r, r * 2.f, r * 2.f, juce::jmax(1.5f, r * 0.08f));
    const float rr = r * (0.9f - pulse * 0.08f);
    g.setGradientFill(juce::ColourGradient(cone.brighter(0.3f), cx - rr * 0.3f, cy - rr * 0.3f, Colour(0xff050505), cx + rr, cy + rr, true));
    g.fillEllipse(cx - rr, cy - rr, rr * 2.f, rr * 2.f);
    g.setColour(ring.withAlpha(0.5f)); g.drawEllipse(cx - rr * 0.6f, cy - rr * 0.6f, rr * 1.2f, rr * 1.2f, 1.2f);
    g.setColour(Colour(0xff0a0a0a)); g.fillEllipse(cx - rr * 0.25f, cy - rr * 0.25f, rr * 0.5f, rr * 0.5f);
    blob(g, cx, cy, r * 1.6f * (0.6f + pulse), ring.withAlpha(juce::jlimit(0.f, 1.f, 0.25f * pulse)));
}

void VfxRenderer::drawWheel(Graphics& g, float cx, float cy, float r, int style, float rot) const
{
    g.setColour(Colour(0xff0c0c0c)); g.fillEllipse(cx - r, cy - r, r * 2.f, r * 2.f);
    for (int k = 0; k < 8; ++k) {
        const float a = rot * 1.3f + (float)k * kTwoPi / 8.f;
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillEllipse(cx + std::cos(a) * r * 0.93f - 1.5f, cy + std::sin(a) * r * 0.93f - 1.5f, 3.f, 3.f);
    }
    const float rr = r * 0.76f;
    g.setGradientFill(juce::ColourGradient(Colour(0xfff0f0f8), cx - rr * 0.5f, cy - rr * 0.5f, Colour(0xff50505c), cx + rr, cy + rr, true));
    g.fillEllipse(cx - rr, cy - rr, rr * 2.f, rr * 2.f);
    g.setColour(Colour(0xff101018)); g.fillEllipse(cx - rr * 0.86f, cy - rr * 0.86f, rr * 1.72f, rr * 1.72f);
    const int n = style == 1 ? 6 : 5;
    for (int i = 0; i < n; ++i) {
        const float a = (style == 2 ? 0.f : rot) + (float)i * kTwoPi / (float)n;
        g.setColour(Colour(0xffd8d8e4));
        if (style == 1) {
            juce::Path p; p.startNewSubPath(cx, cy);
            p.quadraticTo(cx + std::cos(a + 0.6f) * rr * 0.5f, cy + std::sin(a + 0.6f) * rr * 0.5f,
                          cx + std::cos(a + 0.2f) * rr * 0.92f, cy + std::sin(a + 0.2f) * rr * 0.92f);
            g.strokePath(p, juce::PathStrokeType(juce::jmax(2.f, r * 0.07f)));
        } else g.drawLine(cx, cy, cx + std::cos(a) * rr * 0.9f, cy + std::sin(a) * rr * 0.9f, juce::jmax(2.f, r * 0.09f));
    }
    g.setColour(juce::Colours::white.withAlpha(0.25f)); g.drawEllipse(cx - rr * 0.95f, cy - rr * 0.95f, rr * 1.9f, rr * 1.9f, 2.f);
    g.setColour(Colour(0xffe8e8f0)); g.fillEllipse(cx - r * 0.17f, cy - r * 0.17f, r * 0.34f, r * 0.34f);
    g.setColour(Colour(0xff101018)); g.setFont(juce::Font(r * 0.2f, juce::Font::bold));
    g.drawText("H", juce::Rectangle<float>(cx - r * 0.17f, cy - r * 0.17f, r * 0.34f, r * 0.34f).toNearestInt(), juce::Justification::centred, false);
    if (style == 2) { g.setColour(L_->c2.withAlpha(0.6f)); g.drawEllipse(cx - r * 0.24f, cy - r * 0.24f, r * 0.48f, r * 0.48f, 1.5f); }
}

void VfxRenderer::paintCarSide(Graphics& g)
{
    const int rimStyle = (var_ / 3) % 3;
    const float I = L_->intensity;
    const float carW = juce::jmin(W * 0.72f, H * 1.5f), x0 = W * 0.5f - carW * 0.5f, groundY = H * 0.80f;
    auto PX = [&](float u) { return x0 + u * carW; };
    auto PY = [&](float vv) { return groundY - vv * carW; };
    const float wheelR = carW * (0.085f + L_->scale * 0.07f);
    const float rot = t * (rimStyle == 1 ? 9.f : 5.f);
    const float pulse = juce::jlimit(0.f, 1.f, 0.6f + lvl_ * 0.6f);

    for (int i = 0; i < 30; ++i) {
        const float x = W - std::fmod(hash01((uint32_t)(i * 3 + 1)) * W + t * 30.f * (0.3f + hash01((uint32_t)(i * 7))), W);
        blob(g, x, groundY - H * (0.05f + 0.35f * hash01((uint32_t)(i * 5))), 10.f + hash01((uint32_t)(i * 9)) * 20.f,
             pick(hash01((uint32_t)(i * 13))).withAlpha(0.25f * I));
    }
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRect(0.f, groundY, W, H - groundY);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    for (float x = -std::fmod(t * carW * 0.8f, 120.f); x < W; x += 120.f) g.fillRect(x, groundY + (H - groundY) * 0.5f, 60.f, 4.f);
    g.setColour(L_->c2.withAlpha(0.8f * I)); g.drawLine(0.f, groundY, W, groundY, 2.f);

    // underglow
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(1.f, 0.16f, W * 0.5f, groundY));
    blob(g, W * 0.5f, groundY, carW * 0.62f, L_->c2.withAlpha(juce::jlimit(0.f, 1.f, 0.7f * pulse * I)));
    g.restoreState();

    // body
    juce::Path body;
    const float pts[10][2] = { {0.02f, 0.11f}, {0.0f, 0.20f}, {0.07f, 0.26f}, {0.25f, 0.29f}, {0.34f, 0.40f},
                               {0.56f, 0.40f}, {0.70f, 0.30f}, {0.96f, 0.24f}, {1.0f, 0.18f}, {0.99f, 0.11f} };
    body.startNewSubPath(PX(pts[0][0]), PY(pts[0][1]));
    for (int i = 1; i < 10; ++i) body.lineTo(PX(pts[i][0]), PY(pts[i][1]));
    body.closeSubPath();
    body = body.createPathWithRoundedCorners(carW * 0.03f);
    g.setGradientFill(juce::ColourGradient(L_->c1.brighter(0.5f), 0.f, PY(0.4f), L_->c1.darker(0.7f), 0.f, PY(0.1f), false));
    g.fillPath(body);
    g.setColour(L_->c3.withAlpha(0.6f)); g.strokePath(body, juce::PathStrokeType(1.5f));

    // glass + rear window subs
    juce::Path glass;
    glass.startNewSubPath(PX(0.27f), PY(0.30f)); glass.lineTo(PX(0.35f), PY(0.385f));
    glass.lineTo(PX(0.55f), PY(0.385f)); glass.lineTo(PX(0.67f), PY(0.30f)); glass.closeSubPath();
    g.setGradientFill(juce::ColourGradient(Colour(0xff203050).withAlpha(0.9f), 0.f, PY(0.385f), Colour(0xff050a14), 0.f, PY(0.30f), false));
    g.fillPath(glass);
    drawWoofer(g, PX(0.33f), PY(0.335f), carW * 0.03f, lvl_, L_->c2, L_->c1);

    // neon beltline + door details
    g.setColour(L_->c2.withMultipliedAlpha(0.3f)); g.drawLine(PX(0.03f), PY(0.215f), PX(0.97f), PY(0.235f), 8.f);
    g.setColour(L_->c2); g.drawLine(PX(0.03f), PY(0.215f), PX(0.97f), PY(0.235f), 2.5f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(PX(0.45f), PY(0.14f), PX(0.45f), PY(0.30f), 1.2f); g.drawLine(PX(0.66f), PY(0.14f), PX(0.66f), PY(0.30f), 1.2f);
    drawWoofer(g, PX(0.55f), PY(0.185f), carW * 0.032f, lvl_, L_->c2, L_->c1);
    g.setColour(L_->c2.withAlpha(0.9f)); g.setFont(juce::Font(carW * 0.03f, juce::Font::bold));
    g.drawText(L_->text.isEmpty() ? juce::String("CHICAGO BEATS") : L_->text,
               juce::Rectangle<float>(PX(0.10f), PY(0.20f), carW * 0.3f, carW * 0.04f).toNearestInt(), juce::Justification::centred, false);

    // wheels (huge) with wells cut into the body
    for (int w = 0; w < 2; ++w) {
        const float wx = PX(w == 0 ? 0.22f : 0.78f), wy = groundY - wheelR;
        g.setColour(Colour(0xff000000)); g.fillEllipse(wx - wheelR * 1.12f, wy - wheelR * 1.12f, wheelR * 2.24f, wheelR * 2.24f);
        drawWheel(g, wx, wy, wheelR, rimStyle, rot);
    }
    // under-body tube
    g.setColour(L_->c2.withMultipliedAlpha(0.35f)); g.drawLine(PX(0.12f), PY(0.095f), PX(0.88f), PY(0.095f), 8.f);
    g.setColour(L_->c2); g.drawLine(PX(0.12f), PY(0.095f), PX(0.88f), PY(0.095f), 2.5f);

    // headlight + beam, taillight
    const float hx = PX(0.985f), hy = PY(0.215f);
    g.setGradientFill(juce::ColourGradient(L_->c3.withAlpha(0.4f * I), hx, hy, L_->c3.withAlpha(0.f), hx + carW * 0.45f, hy, false));
    juce::Path beam; beam.startNewSubPath(hx, hy); beam.lineTo(hx + carW * 0.45f, hy - carW * 0.05f);
    beam.lineTo(hx + carW * 0.45f, hy + carW * 0.12f); beam.closeSubPath(); g.fillPath(beam);
    g.setColour(L_->c3); g.fillRoundedRectangle(PX(0.93f), PY(0.235f), carW * 0.05f, carW * 0.022f, 3.f);
    blob(g, hx, hy, carW * 0.05f, L_->c3.withAlpha(0.8f * I));
    g.setColour(Colour(0xffff1030)); g.fillRoundedRectangle(PX(0.0f), PY(0.25f), carW * 0.035f, carW * 0.05f, 3.f);
    blob(g, PX(0.01f), PY(0.225f), carW * 0.05f, Colour(0xffff1030).withAlpha(0.7f * I));
}

void VfxRenderer::paintCarInterior(Graphics& g)
{
    const float I = L_->intensity;
    g.setGradientFill(juce::ColourGradient(L_->c1.darker(1.5f), 0.f, 0.f, Colour(0xff000000), 0.f, H * 0.55f, false));
    g.fillRect(0.f, 0.f, W, H * 0.6f);
    const float vx = W * 0.5f, vy = H * 0.32f;
    for (int i = 0; i < 60; ++i) {
        const float ang = hash01((uint32_t)(i * 3 + 1)) * kTwoPi, s = std::fmod(hash01((uint32_t)(i * 7 + 2)) + t * 0.4f, 1.f);
        const float d = s * s * W * 0.7f, x = vx + std::cos(ang) * d, y = vy + std::sin(ang) * d * 0.6f;
        g.setColour(pick(hash01((uint32_t)(i * 11))).withAlpha(juce::jlimit(0.f, 1.f, s * 0.8f * I)));
        g.drawLine(x, y, x + std::cos(ang) * s * 30.f, y + std::sin(ang) * s * 18.f, 1.f + s * 2.f);
    }
    juce::Path dash; dash.startNewSubPath(0.f, H * 0.62f); dash.quadraticTo(W * 0.5f, H * 0.52f, W, H * 0.62f);
    dash.lineTo(W, H); dash.lineTo(0.f, H); dash.closeSubPath();
    g.setGradientFill(juce::ColourGradient(Colour(0xff16161e), 0.f, H * 0.55f, Colour(0xff000000), 0.f, H, false)); g.fillPath(dash);
    juce::Path top; top.startNewSubPath(0.f, H * 0.62f); top.quadraticTo(W * 0.5f, H * 0.52f, W, H * 0.62f);
    g.setColour(L_->c2.withMultipliedAlpha(0.3f)); g.strokePath(top, juce::PathStrokeType(10.f));
    g.setColour(L_->c2); g.strokePath(top, juce::PathStrokeType(3.f));

    // giant TV across centre + passenger side
    const juce::Rectangle<float> tv(W * 0.30f, H * 0.60f, W * 0.62f, H * 0.30f);
    g.setColour(Colour(0xff000000)); g.fillRoundedRectangle(tv, 8.f);
    const int nb = 36; const float bw = tv.getWidth() / (float)nb;
    for (int i = 0; i < nb; ++i) {
        const float hh = tv.getHeight() * 0.85f * (0.12f + 0.88f * std::abs(std::sin(t * 3.f + (float)i * 0.7f + hash01((uint32_t)i) * 6.f)) * (0.3f + lvl_ * 0.9f));
        g.setColour(pick((float)i / (float)nb).withAlpha(juce::jlimit(0.f, 1.f, 0.9f * I)));
        g.fillRect(tv.getX() + (float)i * bw + 1.f, tv.getBottom() - hh - 4.f, bw - 2.f, hh);
    }
    g.setColour(L_->c2.withMultipliedAlpha(0.35f)); g.drawRoundedRectangle(tv, 8.f, 9.f);
    g.setColour(L_->c2); g.drawRoundedRectangle(tv, 8.f, 3.f);
    g.setFont(juce::Font(tv.getHeight() * 0.12f, juce::Font::bold)); g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawText(L_->text.isEmpty() ? juce::String("CHICAGO BEATS") : L_->text, tv.reduced(10.f, 6.f).toNearestInt(), juce::Justification::topLeft, false);

    // steering wheel + gauges
    const float wx = W * 0.17f, wy = H * 0.80f, R = H * 0.16f;
    g.setColour(Colour(0xff0a0a0e)); g.drawEllipse(wx - R, wy - R, R * 2.f, R * 2.f, R * 0.18f);
    g.setColour(L_->c1.withAlpha(0.9f)); g.drawEllipse(wx - R * 0.92f, wy - R * 0.92f, R * 1.84f, R * 1.84f, 2.f);
    for (int k = 0; k < 3; ++k) {
        const float a = kPi * 0.5f + (float)k * kTwoPi / 3.f;
        g.setColour(Colour(0xff0a0a0e)); g.drawLine(wx, wy, wx + std::cos(a) * R * 0.9f, wy + std::sin(a) * R * 0.9f, R * 0.12f);
    }
    g.setColour(L_->c2); g.fillEllipse(wx - R * 0.14f, wy - R * 0.14f, R * 0.28f, R * 0.28f);
    for (int gi = 0; gi < 2; ++gi) {
        const float gx2 = W * (gi ? 0.24f : 0.10f), gy2 = H * 0.66f, gr = H * 0.06f;
        juce::Path arc; arc.addCentredArc(gx2, gy2, gr, gr, 0.f, -2.3f, 2.3f, true);
        g.setColour(L_->c1.withAlpha(0.9f)); g.strokePath(arc, juce::PathStrokeType(3.f));
        const float na = -2.3f + 4.6f * juce::jlimit(0.f, 1.f, 0.2f + lvl_ * 0.7f + 0.1f * std::sin(t * 2.f + (float)gi));
        g.setColour(juce::Colours::white); g.drawLine(gx2, gy2, gx2 + std::sin(na) * gr * 0.9f, gy2 - std::cos(na) * gr * 0.9f, 2.f);
    }
    drawWoofer(g, W * 0.06f, H * 0.90f, H * 0.07f, lvl_, L_->c2, L_->c1);
    drawWoofer(g, W * 0.965f, H * 0.78f, H * 0.045f, lvl_, L_->c2, L_->c1);
    g.setColour(Colour(0xff111118)); g.fillRoundedRectangle(W * 0.44f, H * 0.02f, W * 0.12f, H * 0.07f, 8.f);
    g.setColour(L_->c2.withAlpha(0.7f)); g.drawRoundedRectangle(W * 0.44f, H * 0.02f, W * 0.12f, H * 0.07f, 8.f, 1.5f);
}

void VfxRenderer::paintCarTrunk(Graphics& g)
{
    const float I = L_->intensity;
    g.setGradientFill(juce::ColourGradient(L_->c1.darker(1.2f), 0.f, 0.f, Colour(0xff000000), 0.f, H, false)); g.fillRect(0.f, 0.f, W, H);
    const juce::Rectangle<float> fr(W * 0.06f, H * 0.08f, W * 0.88f, H * 0.72f);
    g.setColour(L_->c2.withMultipliedAlpha(0.3f)); g.drawRoundedRectangle(fr, 18.f, 12.f);
    g.setColour(L_->c2); g.drawRoundedRectangle(fr, 18.f, 3.5f);
    const float cw = fr.getWidth() / 3.f, ch = fr.getHeight() / 2.f, rad = juce::jmin(cw, ch) * 0.42f;
    for (int r = 0; r < 2; ++r) for (int c = 0; c < 3; ++c)
        drawWoofer(g, fr.getX() + ((float)c + 0.5f) * cw, fr.getY() + ((float)r + 0.5f) * ch, rad,
                   juce::jlimit(0.f, 1.f, lvl_ * (0.8f + 0.2f * std::sin(t * 9.f + (float)(r * 3 + c)))), L_->c2, L_->c1);
    for (int i = 0; i < 32; ++i) {
        g.setColour(pick((float)i / 32.f).withAlpha(hash01((uint32_t)(i * 7 + (int)(t * 6.f))) > 0.4f ? 0.9f * I : 0.15f));
        g.fillRect(fr.getX() + (float)i * fr.getWidth() / 32.f + 2.f, H * 0.84f, fr.getWidth() / 32.f - 4.f, 6.f);
    }
    const juce::Rectangle<float> pl(W * 0.5f - W * 0.09f, H * 0.89f, W * 0.18f, H * 0.08f);
    g.setColour(juce::Colours::white); g.fillRoundedRectangle(pl, 6.f);
    g.setColour(juce::Colours::black); g.setFont(juce::Font(pl.getHeight() * 0.55f, juce::Font::bold));
    g.drawText(L_->text.isEmpty() ? juce::String("CHICAGO") : L_->text, pl.toNearestInt(), juce::Justification::centred, true);
}

void VfxRenderer::paintCar(Graphics& g)
{
    const int view = var_ % 3;
    if (view == 0) paintCarSide(g); else if (view == 1) paintCarInterior(g); else paintCarTrunk(g);
}
