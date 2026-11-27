#include "Vfx.h"
#include "VfxUtil.h"

// ============================================================================
//  helpers
// ============================================================================
juce::Colour VfxRenderer::pick(float u) const
{
    u -= std::floor(u);
    if (L_->rainbow) return juce::Colour::fromHSV(std::fmod(u + t * 0.02f, 1.f), 0.85f, 1.f, 1.f);
    return u < 0.5f ? L_->c1.interpolatedWith(L_->c2, u * 2.f)
                    : L_->c2.interpolatedWith(L_->c3, (u - 0.5f) * 2.f);
}

void VfxRenderer::blob(juce::Graphics& g, float cx, float cy, float r, juce::Colour c) const
{
    if (r < 0.5f || c.isTransparent()) return;
    g.setColour(c);
    const int d = (int)(r * 2.f);
    g.drawImage(softBlob(), (int)(cx - r), (int)(cy - r), d, d, 0, 0, 64, 64, true);
}

void VfxRenderer::reset()
{
    ps.clear(); bolts.clear(); flash = 0;
    for (auto& x : tm) x = 0;
    keyLayoutW = 0; atlasVar = -1; loadedPath = "\x01"; // force media reload
    for (auto& k : keyGlow) k = 0;

    switch (L_->type)
    {
    case VfxGalaxy: {
        const int n = 300 + (int)(L_->density * 900.f);
        for (int i = 0; i < n; ++i) {
            P p; p.x = rng.nextFloat() * kTwoPi; p.y = rng.nextFloat(); p.vx = 0.2f + rng.nextFloat();
            p.size = 0.8f + rng.nextFloat() * 2.2f; ps.push_back(p);
        }
        break; }
    case VfxLightning: {
        for (int i = 0; i < 14; ++i) {
            P p; p.x = rng.nextFloat() * W; p.y = rng.nextFloat() * H * 0.25f;
            p.size = W * (0.15f + rng.nextFloat() * 0.2f); p.vx = 5.f + rng.nextFloat() * 15.f; ps.push_back(p);
        }
        break; }
    case VfxStarfield: {
        const int n = 150 + (int)(L_->density * 500.f);
        for (int i = 0; i < n; ++i) {
            P p; p.x = rng.nextFloat() * 2.f - 1.f; p.y = rng.nextFloat() * 2.f - 1.f;
            p.aux = 0.05f + rng.nextFloat() * 0.95f; p.u = rng.nextFloat(); p.id = i; ps.push_back(p);
        }
        break; }
    case VfxWisdomTree: {
        for (int i = 0; i < 26; ++i) {
            P p; p.x = rng.nextFloat() * W; p.y = H * (0.2f + rng.nextFloat() * 0.7f);
            p.vx = (rng.nextFloat() - 0.5f) * 14.f; p.vy = (rng.nextFloat() - 0.5f) * 10.f;
            p.u = rng.nextFloat(); p.aux = rng.nextFloat() * kTwoPi; p.size = 3.f + rng.nextFloat() * 4.f; ps.push_back(p);
        }
        break; }
    default: break;
    }
}

void VfxRenderer::paint(juce::Graphics& g, juce::Rectangle<float> area, const SkinLayer& layer, const VfxContext& ctx)
{
    if (area.getWidth() < 8.f || area.getHeight() < 8.f) return;
    L_ = &layer; c_ = &ctx; W = area.getWidth(); H = area.getHeight(); var_ = layer.variant;

    if (layer.type != lastType || layer.variant != lastVariant || std::abs(W - lastW) > 1.f || std::abs(H - lastH) > 1.f) {
        lastType = layer.type; lastVariant = layer.variant; lastW = W; lastH = H;
        reset();
    }
    dt_ = juce::jlimit(0.f, 0.1f, ctx.dt) * layer.speed;
    t += dt_;
    lvl_ = juce::jlimit(0.f, 1.f, ctx.level) * layer.react;

    newNotes.clear();
    for (int i = 0; i < 128; ++i) {
        const float v = ctx.noteVel[i];
        if (v > 0.f && prevVel[i] <= 0.f) newNotes.push_back(i);
        prevVel[i] = v; curVel[i] = v;
    }

    const bool tl = layer.opacity < 0.99f;
    g.saveState();
    g.reduceClipRegion(juce::Rectangle<int>(0, 0, (int)std::ceil(W), (int)std::ceil(H)));
    if (tl) g.beginTransparencyLayer(juce::jlimit(0.f, 1.f, layer.opacity));

    switch (layer.type)
    {
    case VfxGalaxy:      paintGalaxy(g); break;
    case VfxMatrix:      paintMatrix(g); break;
    case VfxFire:        paintFire(g); break;
    case VfxPyro:        paintPyro(g); break;
    case VfxLightning:   paintLightning(g); break;
    case VfxWater:       paintWater(g); break;
    case VfxTropical:    paintTropical(g); break;
    case VfxWisdomTree:  paintTrees(g); break;
    case VfxNeonNotes:   paintNeon(g); break;
    case VfxStudio:      paintStudio(g); break;
    case VfxCar:         paintCar(g); break;
    case VfxAurora:      paintAurora(g); break;
    case VfxStarfield:   paintStarfield(g); break;
    case VfxMedia:       paintMedia(g); break;
    case VfxGradient:    paintGradient(g); break;
    default: break;
    }

    if (tl) g.endTransparencyLayer();
    g.restoreState();
}

// ============================================================================
//  Galaxy
// ============================================================================
void VfxRenderer::paintGalaxy(juce::Graphics& g)
{
    const float l1 = c_->lfo1, l2 = c_->lfo2, lev = lvl_;
    const float twist = 3.f + (float)(var_ % 15) * 0.6f;
    const float tilt = 0.3f + (float)((var_ / 5) % 3) * 0.3f;
    const float I = L_->intensity;

    gRot += (0.12f + std::abs(l1) * 0.6f + lev * 1.5f) * dt_;
    gSc += ((0.8f + lev * 0.7f) - gSc) * juce::jmin(1.f, dt_ * 7.f);
    gvx += (l1 * 30.f - gx) * 0.08f; gvy += (l2 * 30.f - gy) * 0.08f;
    gvx *= 0.85f; gvy *= 0.85f; gx += gvx; gy += gvy;

    const float cx = W * 0.5f + gx, cy = H * 0.5f + gy;
    const float R = juce::jmin(W, H) * 0.45f * gSc * (0.6f + L_->scale * 0.8f);

    g.setGradientFill(juce::ColourGradient(pick(0.f).withAlpha(juce::jlimit(0.f, 1.f, (0.5f + lev * 0.5f) * I)), cx, cy,
                                           pick(0.f).withAlpha(0.f), cx + R, cy, true));
    g.fillEllipse(cx - R, cy - R, R * 2.f, R * 2.f);

    for (auto& s : ps) {
        s.x += s.vx * 0.06f * dt_ * (1.f + std::abs(l2) * 4.f);
        const float a = s.x + s.y * twist + gRot;
        const float rr = s.y * R;
        const float x = cx + std::cos(a) * rr, y = cy + std::sin(a) * rr * tilt;
        g.setColour(pick(s.y * 0.95f).withAlpha(juce::jlimit(0.f, 1.f, (0.25f + 0.6f * (1.f - s.y) + lev * 0.2f) * I)));
        g.fillEllipse(x, y, s.size, s.size);
    }
    g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.f, 1.f, 0.6f + lev * 0.4f)));
    g.fillEllipse(cx - 6.f - lev * 10.f, cy - 4.f - lev * 6.f, 12.f + lev * 20.f, 8.f + lev * 12.f);
}

// ============================================================================
//  Matrix code (coloured, any palette or rainbow)
// ============================================================================
void VfxRenderer::ensureAtlas(int var, float fontPx, int cell)
{
    if (atlas.isValid() && atlasVar == var && atlasCell == cell && std::abs(atlasPx - fontPx) < 0.01f) return;
    juce::String gl;
    switch (var) {
        case 0: for (int cp = 0xFF71; cp <= 0xFF9D; ++cp) gl += juce::String::charToString((juce_wchar)cp);
                gl += "0123456789"; break;
        case 1: gl = "01"; break;
        case 2: gl = "0123456789ABCDEF"; break;
        case 3: gl = "<>/\\{}[]=+*#@$%&;:!?|~^"; break;
        default: gl = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; break;
    }
    atlasN = juce::jmax(1, gl.length());
    atlas = juce::Image(juce::Image::ARGB, cell * atlasN, cell, true);
    juce::Graphics ag(atlas);
    ag.setColour(juce::Colours::white);
    ag.setFont(juce::Font(var == 0 ? "MS Gothic" : "Consolas", fontPx, juce::Font::plain));
    for (int i = 0; i < atlasN; ++i)
        ag.drawText(juce::String::charToString(gl[i]), i * cell, 0, cell, cell, juce::Justification::centred, false);
    atlasVar = var; atlasCell = cell; atlasPx = fontPx;
}

void VfxRenderer::paintMatrix(juce::Graphics& g)
{
    const int v = var_ % 5;
    const float fontPx = 10.f + L_->scale * 26.f;
    const int cell = juce::jmax(8, (int)(fontPx * 1.15f));
    ensureAtlas(v, fontPx, cell);

    const int cols = (int)(W / (float)cell) + 1;
    if ((int)ps.size() != cols) {
        ps.assign((size_t)cols, P());
        for (int i = 0; i < cols; ++i) {
            auto& p = ps[(size_t)i];
            p.id = i; p.x = (float)(i * cell); p.y = -rng.nextFloat() * H * 1.2f;
            p.vy = 6.f + rng.nextFloat() * 16.f; p.size = 8.f + rng.nextFloat() * 22.f; p.aux = rng.nextFloat();
        }
    }
    const float thresh = juce::jlimit(0.05f, 1.f, L_->density * 0.85f + 0.15f);
    const float flick = t * 4.f;
    const float I = L_->intensity;

    for (auto& p : ps) {
        p.y += p.vy * dt_ * (float)cell * (1.f + lvl_ * 1.5f);
        if (p.y - p.size * (float)cell > H) {
            p.y = -rng.nextFloat() * H * 0.4f; p.vy = 6.f + rng.nextFloat() * 16.f;
            p.size = 8.f + rng.nextFloat() * 22.f; p.aux = rng.nextFloat();
        }
        if (p.aux > thresh) continue;
        const int headRow = (int)(p.y / (float)cell);
        const int len = juce::jmax(2, (int)p.size);
        for (int k = 0; k < len; ++k) {
            const int row = headRow - k;
            if (row < 0) continue;
            const float yy = (float)(row * cell);
            if (yy > H) continue;
            const float f = (float)k / (float)len;
            const float a = (1.f - f) * (1.f - f);
            const float u = L_->rainbow ? ((float)p.id * 0.0137f + f * 0.12f) : (0.02f + f * 0.95f);
            juce::Colour col = pick(u);
            if (k == 0) col = col.interpolatedWith(juce::Colours::white, 0.65f);
            const int fl = (int)(flick * (k < 3 ? 3.f : 0.25f));
            const int gi = (int)(hash32((uint32_t)(p.id * 7919 + row * 104729 + fl * 13)) % (uint32_t)atlasN);
            g.setColour(col.withAlpha(juce::jlimit(0.f, 1.f, a * I)));
            g.drawImage(atlas, (int)p.x, (int)yy, cell, cell, gi * cell, 0, cell, cell, true);
        }
    }
}

// ============================================================================
//  Fire
// ============================================================================
void VfxRenderer::paintFire(juce::Graphics& g)
{
    const int v = var_ % 4;
    const float I = L_->intensity;

    g.setGradientFill(juce::ColourGradient(pick(0.25f).withAlpha(juce::jlimit(0.f, 1.f, 0.35f * I + lvl_ * 0.2f)), 0.f, H,
                                           pick(0.25f).withAlpha(0.f), 0.f, H * 0.55f, false));
    g.fillRect(0.f, 0.f, W, H);

    tm[0] += dt_ * (80.f + L_->density * 420.f);
    while (tm[0] >= 1.f && ps.size() < 1000) {
        tm[0] -= 1.f;
        P p;
        const float r3 = (rng.nextFloat() + rng.nextFloat() + rng.nextFloat()) / 3.f - 0.5f;
        if (v == 0 || v == 3) p.x = rng.nextFloat() * W;
        else if (v == 1) p.x = W * 0.5f + r3 * W * 0.4f;
        else p.x = rng.nextBool() ? rng.nextFloat() * W * 0.15f : W - rng.nextFloat() * W * 0.15f;
        p.y = H + 10.f;
        p.vy = -(60.f + rng.nextFloat() * 140.f) * (0.6f + L_->scale) * (1.f + lvl_ * 1.2f);
        p.vx = (rng.nextFloat() - 0.5f) * 40.f;
        p.max = v == 3 ? 2.f + rng.nextFloat() * 3.f : 1.f + rng.nextFloat() * 1.6f;
        p.life = p.max;
        p.size = v == 3 ? 3.f + rng.nextFloat() * 5.f : (24.f + rng.nextFloat() * 60.f) * (0.4f + L_->scale * 1.2f);
        p.id = rng.nextInt(1000);
        ps.push_back(p);
    }
    for (int i = (int)ps.size() - 1; i >= 0; --i) {
        P& p = ps[(size_t)i];
        p.life -= dt_;
        if (p.life <= 0.f) { ps[(size_t)i] = ps.back(); ps.pop_back(); continue; }
        p.x += (p.vx + std::sin(t * 3.f + (float)p.id) * 20.f) * dt_;
        p.y += p.vy * dt_;
        p.vy *= (1.f - 0.3f * dt_);
        if (v != 3) p.size *= (1.f - 0.35f * dt_);
        const float u = 1.f - p.life / p.max;
        const float alpha = std::pow(1.f - u, 1.2f) * (v == 3 ? 1.1f : 0.55f) * I;
        blob(g, p.x, p.y, p.size, pick(u).withAlpha(juce::jlimit(0.f, 1.f, alpha)));
    }
}

// ============================================================================
//  Pyro fireworks (launch on timer AND whenever you play a note)
// ============================================================================
void VfxRenderer::paintPyro(juce::Graphics& g)
{
    const int style = var_ % 4;
    const float I = L_->intensity;

    auto launch = [&](float x, float u) {
        P p; p.kind = 0; p.x = x; p.y = H; p.vx = (rng.nextFloat() - 0.5f) * 30.f;
        p.vy = -(H * (0.55f + rng.nextFloat() * 0.25f));
        p.life = 0.8f + rng.nextFloat() * 0.5f; p.max = p.life; p.u = u; p.size = 3.f;
        ps.push_back(p);
    };
    tm[0] -= dt_;
    if (tm[0] <= 0.f) {
        launch(W * (0.1f + 0.8f * rng.nextFloat()), rng.nextFloat());
        tm[0] = (0.5f + rng.nextFloat() * 1.5f) / (0.3f + L_->density * 2.5f);
    }
    for (int n : newNotes) {
        const int nn = juce::jlimit(21, 108, n);
        launch(W * ((float)(nn - 21) / 87.f), (float)(n % 12) / 12.f);
    }

    const float grav = 260.f * (0.5f + L_->scale * 0.5f);
    std::vector<P> add;
    for (int i = (int)ps.size() - 1; i >= 0; --i) {
        P& p = ps[(size_t)i];
        p.life -= dt_;
        p.x += p.vx * dt_; p.y += p.vy * dt_;
        if (p.kind == 0) {
            p.vy += 60.f * dt_;
            if (ps.size() + add.size() < 1800) {
                P s; s.kind = 2; s.x = p.x; s.y = p.y; s.vx = (rng.nextFloat() - 0.5f) * 20.f;
                s.vy = 20.f + rng.nextFloat() * 40.f; s.life = s.max = 0.4f; s.u = p.u; add.push_back(s);
            }
            if (p.life <= 0.f) {
                if (ps.size() + add.size() < 1500) {
                    const int nSp = 50 + (int)(L_->density * 120.f);
                    const float spd = 260.f * (0.5f + L_->scale);
                    for (int k = 0; k < nSp; ++k) {
                        P s; s.kind = 1; s.x = p.x; s.y = p.y;
                        const float ang = style == 1 ? ((float)k / (float)nSp) * kTwoPi : rng.nextFloat() * kTwoPi;
                        const float sp = style == 1 ? spd : spd * (0.25f + rng.nextFloat() * 0.75f);
                        s.vx = std::cos(ang) * sp; s.vy = std::sin(ang) * sp;
                        s.max = s.life = style == 2 ? 2.2f + rng.nextFloat() * 1.8f : 1.f + rng.nextFloat() * 1.2f;
                        s.u = style == 3 ? (float)k / (float)nSp : (style == 2 ? 0.15f : p.u + (rng.nextFloat() - 0.5f) * 0.1f);
                        s.size = 5.f + rng.nextFloat() * 4.f;
                        add.push_back(s);
                    }
                }
                flash = juce::jmin(1.f, flash + 0.5f);
                ps[(size_t)i] = ps.back(); ps.pop_back();
                continue;
            }
        } else {
            if (p.kind == 1) { p.vy += grav * (style == 2 ? 1.6f : 1.f) * dt_; p.vx *= (1.f - 0.8f * dt_); }
            if (p.life <= 0.f) { ps[(size_t)i] = ps.back(); ps.pop_back(); continue; }
        }
    }
    ps.insert(ps.end(), add.begin(), add.end());

    for (auto& p : ps) {
        const float a = juce::jlimit(0.f, 1.f, p.life / p.max);
        if (p.kind == 0) {
            blob(g, p.x, p.y, 7.f, juce::Colours::white.withAlpha(0.9f * I));
        } else if (p.kind == 1) {
            const auto col = pick(p.u);
            g.setColour(col.withAlpha(a * 0.7f * I));
            g.drawLine(p.x, p.y, p.x - p.vx * 0.05f, p.y - p.vy * 0.05f, 2.f);
            blob(g, p.x, p.y, p.size * (0.7f + L_->scale), col.withAlpha(juce::jlimit(0.f, 1.f, a * I)));
        } else {
            blob(g, p.x, p.y, 4.f, pick(p.u).withAlpha(a * 0.6f * I));
        }
    }
    if (flash > 0.01f) {
        g.setColour(pick(0.3f).withAlpha(juce::jlimit(0.f, 1.f, flash * 0.12f * I)));
        g.fillRect(0.f, 0.f, W, H);
        flash = juce::jmax(0.f, flash - dt_ * 1.5f);
    }
}

// ============================================================================
//  Lightning / Thunder
// ============================================================================
void VfxRenderer::genBolt(std::vector<Seg>& out, float x1, float y1, float x2, float y2, float disp, int depth, float w)
{
    const float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy);
    if (depth <= 0 || len < 8.f) { out.push_back({ x1, y1, x2, y2, w }); return; }
    const float nx = -dy / len, ny = dx / len;
    const float off = (rng.nextFloat() - 0.5f) * 2.f * disp;
    const float mx = (x1 + x2) * 0.5f + nx * off, my = (y1 + y2) * 0.5f + ny * off;
    genBolt(out, x1, y1, mx, my, disp * 0.55f, depth - 1, w);
    genBolt(out, mx, my, x2, y2, disp * 0.55f, depth - 1, w);
    if (depth >= 3 && rng.nextFloat() < 0.22f) {
        const float bx = mx + dx * 0.5f + (rng.nextFloat() - 0.5f) * len;
        const float by = my + dy * 0.5f + (rng.nextFloat() - 0.5f) * len * 0.5f;
        genBolt(out, mx, my, bx, by, disp * 0.5f, depth - 2, w * 0.55f);
    }
}

void VfxRenderer::paintLightning(juce::Graphics& g)
{
    const int v = var_ % 4;
    const float I = L_->intensity;

    if (v <= 1) {
        const auto cloudCol = L_->c3.interpolatedWith(L_->c1, juce::jlimit(0.f, 1.f, flash));
        for (auto& c : ps) {
            c.x += c.vx * dt_; if (c.x - c.size > W) c.x = -c.size;
            blob(g, c.x, c.y, c.size, cloudCol.withAlpha(juce::jlimit(0.f, 1.f, 0.35f * I + flash * 0.3f)));
        }
        const int N = 40 + (int)(L_->density * 160.f);
        g.setColour(L_->c1.withAlpha(0.22f * I));
        for (int i = 0; i < N; ++i) {
            const float x = std::fmod(hash01((uint32_t)(i * 7 + 1)) * W + t * 60.f, W + 20.f);
            const float y = std::fmod(hash01((uint32_t)(i * 13 + 5)) * H + t * H * 0.9f * (0.7f + hash01((uint32_t)i)), H);
            g.drawLine(x, y, x - 5.f, y + 18.f, 1.f);
        }
    }

    auto strike = [&]() {
        Bolt b; b.col = pick(rng.nextFloat() * 0.4f);
        const float disp = W * 0.07f * (0.4f + L_->scale);
        if (v == 0) {
            const float x0 = W * (0.1f + rng.nextFloat() * 0.8f);
            genBolt(b.segs, x0, 0.f, x0 + (rng.nextFloat() - 0.5f) * W * 0.3f, H * (0.7f + rng.nextFloat() * 0.3f), disp, 6, 1.f);
            b.life = b.max = 0.35f + rng.nextFloat() * 0.25f; flash = 1.f;
        } else if (v == 1) {
            for (int k = 0; k < 3; ++k) {
                const float x0 = W * (0.1f + rng.nextFloat() * 0.8f);
                genBolt(b.segs, x0, 0.f, x0 + (rng.nextFloat() - 0.5f) * W * 0.3f, H * (0.6f + rng.nextFloat() * 0.4f), disp, 6, 1.f);
            }
            b.life = b.max = 0.4f; flash = 1.f;
        } else if (v == 2) {
            const float cx = W * 0.5f, cy = H * 0.5f;
            for (int k = 0; k < 5; ++k) {
                const float a = rng.nextFloat() * kTwoPi, R = juce::jmin(W, H) * (0.25f + rng.nextFloat() * 0.2f);
                genBolt(b.segs, cx, cy, cx + std::cos(a) * R, cy + std::sin(a) * R, disp * 0.35f, 5, 0.8f);
            }
            b.life = b.max = 0.12f; flash = juce::jmax(flash, 0.2f);
        } else {
            for (int k = 0; k < 3; ++k)
                genBolt(b.segs, 0.f, H * rng.nextFloat(), W, H * rng.nextFloat(), disp * 0.4f, 6, 0.8f);
            b.life = b.max = 0.12f; flash = juce::jmax(flash, 0.15f);
        }
        bolts.push_back(b);
        if (bolts.size() > 10) bolts.erase(bolts.begin());
    };

    tm[0] -= dt_; tm[1] -= dt_;
    const bool bass = lvl_ > 0.55f && tm[1] <= 0.f && L_->react > 0.2f;
    if (tm[0] <= 0.f || bass) {
        strike();
        tm[0] = v >= 2 ? 0.07f + rng.nextFloat() * 0.1f : (0.5f + rng.nextFloat() * 2.5f) / (0.3f + L_->density * 2.f);
        if (bass) tm[1] = 0.5f;
    }

    const float widths[3] = { 7.f, 3.f, 1.2f }, alphas[3] = { 0.12f, 0.3f, 1.f };
    for (int i = (int)bolts.size() - 1; i >= 0; --i) {
        Bolt& b = bolts[(size_t)i];
        b.life -= dt_;
        if (b.life <= 0.f) { bolts.erase(bolts.begin() + i); continue; }
        const float a = juce::jlimit(0.f, 1.f, b.life / b.max) * (0.55f + 0.45f * rng.nextFloat()) * I;
        for (int pass = 0; pass < 3; ++pass) {
            g.setColour((pass == 2 ? b.col.interpolatedWith(juce::Colours::white, 0.85f) : b.col)
                            .withAlpha(juce::jlimit(0.f, 1.f, a * alphas[pass])));
            for (auto& s : b.segs) g.drawLine(s.x1, s.y1, s.x2, s.y2, s.w * widths[pass] * (0.7f + L_->scale));
        }
    }
    if (flash > 0.01f) {
        g.setColour(L_->c1.withAlpha(juce::jlimit(0.f, 1.f, flash * 0.22f * I)));
        g.fillRect(0.f, 0.f, W, H);
        flash = juce::jmax(0.f, flash - dt_ * 3.5f);
    }
}

// ============================================================================
//  Water ripples
// ============================================================================
void VfxRenderer::paintWater(juce::Graphics& g)
{
    const int v = var_ % 4;
    const float I = L_->intensity;
    const float ratio = v == 1 ? 0.38f : 1.f;

    g.setGradientFill(juce::ColourGradient(L_->c2.darker(0.8f).withAlpha(0.55f * I), 0.f, 0.f,
                                           L_->c3.withAlpha(0.9f * I), 0.f, H, false));
    g.fillRect(0.f, 0.f, W, H);

    auto drop = [&](float x, float y, float u, bool falling) {
        P p; p.x = x; p.y = y; p.u = u; p.life = falling ? -0.4f : 0.f; p.max = 3.5f + rng.nextFloat() * 2.f;
        if (ps.size() < 160) ps.push_back(p);
    };
    tm[0] -= dt_;
    if (tm[0] <= 0.f) {
        const float y = v == 1 ? H * (0.45f + rng.nextFloat() * 0.5f) : H * (0.15f + rng.nextFloat() * 0.8f);
        drop(rng.nextFloat() * W, y, rng.nextFloat(), v == 0 || v == 2);
        tm[0] = (0.7f + rng.nextFloat()) / (0.4f + L_->density * 3.f) * (v == 2 ? 0.25f : 1.f);
    }
    for (int n : newNotes) {
        const int nn = juce::jlimit(21, 108, n);
        drop(W * ((float)(nn - 21) / 87.f), H * (0.3f + 0.6f * rng.nextFloat()), (float)(n % 12) / 12.f, true);
    }

    if (v == 2) {
        g.setColour(L_->c1.withAlpha(0.18f * I));
        for (int i = 0; i < 80; ++i) {
            const float x = hash01((uint32_t)(i * 5 + 3)) * W;
            const float y = std::fmod(hash01((uint32_t)(i * 11 + 7)) * H + t * H * 1.1f, H);
            g.drawLine(x, y, x - 3.f, y + 14.f, 1.f);
        }
    }

    const float spd = 70.f + L_->scale * 220.f;
    for (int i = (int)ps.size() - 1; i >= 0; --i) {
        P& p = ps[(size_t)i];
        p.life += dt_;
        if (p.life > p.max) { ps[(size_t)i] = ps.back(); ps.pop_back(); continue; }
        if (p.life < 0.f) {
            const float fr = juce::jlimit(0.f, 1.f, (p.life + 0.4f) / 0.4f);
            blob(g, p.x, p.y * fr * fr, 7.f, L_->c1.withAlpha(0.9f * I));
            continue;
        }
        const float fade = 1.f - p.life / p.max;
        const float r0 = p.life * spd;
        for (int k = 0; k < 5; ++k) {
            const float r = r0 - (float)k * (16.f + L_->scale * 20.f);
            if (r <= 2.f) continue;
            const float a = fade * (1.f - (float)k * 0.15f) * I;
            g.setColour(L_->c3.darker(0.6f).withAlpha(juce::jlimit(0.f, 1.f, a * 0.55f)));
            g.drawEllipse(p.x - r, p.y - r * ratio, r * 2.f, r * 2.f * ratio, 6.f);
            g.setColour(pick(p.u * 0.5f + (float)k * 0.07f).withAlpha(juce::jlimit(0.f, 1.f, a)));
            g.drawEllipse(p.x - r, p.y - r * ratio, r * 2.f, r * 2.f * ratio, 1.8f);
        }
        if (p.life < 0.45f) {
            const float jet = std::sin(p.life / 0.45f * kPi) * 50.f * (0.5f + L_->scale);
            blob(g, p.x, p.y - jet, 9.f, L_->c1.withAlpha(0.9f * I));
        }
    }
}

// ============================================================================
//  Neon notes (Keysight-style) - falling demo notes + your live notes float up
// ============================================================================
void VfxRenderer::layoutKeys(float width)
{
    keyLayoutW = width;
    const float ww = width / 52.f; keyW = ww;
    int white = 0;
    for (int n = 21; n <= 108; ++n) {
        const int pc = n % 12;
        keyBlack[n] = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        if (!keyBlack[n]) { keyX[n] = ((float)white + 0.5f) * ww; ++white; }
    }
    for (int n = 22; n <= 107; ++n) if (keyBlack[n]) keyX[n] = (keyX[n - 1] + keyX[n + 1]) * 0.5f;
}

juce::Colour VfxRenderer::noteColour(int n) const
{
    const float u = (float)(juce::jlimit(21, 108, n) - 21) / 87.f;
    return L_->rainbow ? juce::Colour::fromHSV(std::fmod(u * 0.85f + t * 0.02f, 1.f), 0.85f, 1.f, 1.f) : pick(u);
}

void VfxRenderer::paintNeon(juce::Graphics& g)
{
    if (std::abs(keyLayoutW - W) > 0.5f) layoutKeys(W);
    const int style = var_ % 3;
    const float I = L_->intensity;
    const float kbH = H * 0.14f, lineY = H - kbH;
    const float speedPx = H * 0.28f;
    const float bw = keyW * (0.35f + L_->scale * 0.9f);

    if (style == 2) {
        g.setGradientFill(juce::ColourGradient(pick(0.5f).withAlpha(0.35f * I), W * 0.7f, H * 0.4f,
                                               pick(0.5f).withAlpha(0.f), W * 0.2f, H * 0.5f, true));
        g.fillRect(0.f, 0.f, W, H);
    }

    for (auto& k : keyGlow) k = juce::jmax(0.f, k - dt_ * 5.f);

    // demo melody
    tm[0] -= dt_;
    if (tm[0] <= 0.f && L_->density > 0.02f) {
        static const int deg[5] = { 0, 3, 5, 7, 10 };
        const int cnt = 1 + rng.nextInt(3), base = 48 + 12 * rng.nextInt(4);
        const float dur = 0.15f + rng.nextFloat() * 1.1f;
        for (int k = 0; k < cnt; ++k) {
            P p; p.kind = 0; p.id = juce::jlimit(21, 108, base + deg[rng.nextInt(5)] + (k == 1 ? 12 : 0));
            p.x = lineY; p.y = lineY + dur * speedPx; ps.push_back(p);
        }
        tm[0] = (0.12f + rng.nextFloat() * 0.35f) / (0.3f + L_->density * 2.f);
    }
    // live notes
    for (int n : newNotes) {
        P p; p.kind = 1; p.id = juce::jlimit(21, 108, n); p.vx = (float)n; p.x = 0.f; p.y = 0.f; ps.push_back(p);
    }

    for (int i = (int)ps.size() - 1; i >= 0; --i) {
        P& p = ps[(size_t)i];
        if (p.kind == 0) {
            p.x -= speedPx * dt_; p.y -= speedPx * dt_;
            if (p.y <= 0.f) { ps[(size_t)i] = ps.back(); ps.pop_back(); continue; }
            if (p.x <= 0.f) {
                keyGlow[p.id] = 1.f;
                if (rng.nextFloat() < dt_ * 20.f && ps.size() < 600) {
                    P s; s.kind = 3; s.x = keyX[p.id]; s.y = lineY; s.vx = (rng.nextFloat() - 0.5f) * 80.f;
                    s.vy = -(80.f + rng.nextFloat() * 140.f); s.life = s.max = 0.6f; s.id = p.id; ps.push_back(s);
                }
            }
        } else if (p.kind == 1) {
            p.y += speedPx * 1.2f * dt_; keyGlow[p.id] = 1.f;
            if (curVel[juce::jlimit(0, 127, (int)p.vx)] <= 0.f) p.kind = 2;
        } else if (p.kind == 2) {
            p.x += speedPx * 1.2f * dt_; p.y += speedPx * 1.2f * dt_;
            if (p.x > lineY) { ps[(size_t)i] = ps.back(); ps.pop_back(); continue; }
        } else {
            p.life -= dt_;
            if (p.life <= 0.f) { ps[(size_t)i] = ps.back(); ps.pop_back(); continue; }
            p.x += p.vx * dt_; p.y += p.vy * dt_; p.vy += 220.f * dt_;
        }
    }

    // bars
    for (auto& p : ps) {
        if (p.kind == 3) continue;
        const float top = juce::jmax(-20.f, lineY - p.y), bot = lineY - p.x;
        if (bot < 0.f || bot - top < 2.f) continue;
        const auto col = noteColour(p.id);
        const juce::Rectangle<float> r(keyX[p.id] - bw * 0.5f, top, bw, bot - top);
        const float cs = juce::jmin(bw * 0.5f, r.getHeight() * 0.5f);
        if (style == 0) {
            g.setColour(col.withAlpha(0.18f * I)); g.fillRoundedRectangle(r, cs);
            g.setColour(col.withAlpha(0.25f * I)); g.drawRoundedRectangle(r.expanded(2.f), cs + 2.f, 5.f);
            g.setColour(col.withAlpha(juce::jlimit(0.f, 1.f, I))); g.drawRoundedRectangle(r, cs, 2.2f);
        } else if (style == 1) {
            blob(g, r.getCentreX(), r.getCentreY(), juce::jmax(bw * 2.2f, r.getHeight() * 0.35f), col.withAlpha(0.25f * I));
            g.setColour(col.withAlpha(juce::jlimit(0.f, 1.f, 0.9f * I))); g.fillRoundedRectangle(r, cs * 0.4f);
            g.setColour(juce::Colours::white.withAlpha(0.9f * I)); g.drawRoundedRectangle(r, cs * 0.4f, 1.5f);
        } else {
            blob(g, r.getCentreX(), r.getCentreY(), juce::jmax(70.f, r.getHeight() * 0.5f), col.withAlpha(0.18f * I));
            g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(r, cs * 0.6f);
            g.setColour(col.darker(0.5f).withAlpha(juce::jlimit(0.f, 1.f, I))); g.drawRoundedRectangle(r, cs * 0.6f, 2.5f);
        }
    }
    for (auto& p : ps) {
        if (p.kind != 3) continue;
        blob(g, p.x, p.y, 5.f, noteColour(p.id).withAlpha(juce::jlimit(0.f, 1.f, p.life / p.max) * I));
    }

    // glowing hit line
    for (float x = 0.f; x < W; x += 8.f) {
        g.setColour(noteColour(21 + (int)(x / W * 87.f)).withAlpha(0.8f * I));
        g.fillRect(x, lineY - 1.5f, 8.f, 3.f);
    }
    // keyboard
    for (int n = 21; n <= 108; ++n) {
        if (keyBlack[n]) continue;
        const float gl = keyGlow[n];
        g.setColour(juce::Colour(0xff0a0a12).interpolatedWith(noteColour(n), 0.14f + gl * 0.85f));
        g.fillRect(keyX[n] - keyW * 0.5f + 0.5f, lineY, keyW - 1.f, kbH);
    }
    for (int n = 21; n <= 108; ++n) {
        if (!keyBlack[n]) continue;
        const float gl = keyGlow[n];
        g.setColour(juce::Colour(0xff020204).interpolatedWith(noteColour(n), gl * 0.9f));
        g.fillRect(keyX[n] - keyW * 0.3f, lineY, keyW * 0.6f, kbH * 0.62f);
        if (gl > 0.05f) blob(g, keyX[n], lineY, keyW * 2.2f, noteColour(n).withAlpha(gl * 0.5f * I));
    }
    for (int n = 21; n <= 108; ++n)
        if (!keyBlack[n] && keyGlow[n] > 0.05f) blob(g, keyX[n], lineY, keyW * 2.8f, noteColour(n).withAlpha(keyGlow[n] * 0.55f * I));
}

// ============================================================================
//  Aurora
// ============================================================================
void VfxRenderer::paintAurora(juce::Graphics& g)
{
    const float I = L_->intensity;
    const int bands = 3 + (var_ % 10) % 5 + 1;
    const float amp = H * (0.06f + L_->scale * 0.12f) * (1.f + lvl_ * 0.8f);
    for (int i = 0; i < 90; ++i) {
        g.setColour(juce::Colours::white.withAlpha(0.15f + 0.4f * hash01((uint32_t)(i * 3 + 1))));
        g.fillRect(hash01((uint32_t)(i * 17 + 2)) * W, hash01((uint32_t)(i * 29 + 4)) * H * 0.6f, 1.5f, 1.5f);
    }
    for (int k = 0; k < bands; ++k) {
        const float f = (float)k / (float)bands, base = H * (0.25f + 0.35f * f);
        const float thick = H * (0.10f + L_->scale * 0.15f);
        auto yTop = [&](float x) {
            return base + std::sin(x * 0.004f * (1.f + (float)k * 0.3f) + t * 0.5f * (1.f + (float)k * 0.2f) + (float)k * 1.7f) * amp
                        + std::sin(x * 0.011f + t * 0.9f + (float)k) * amp * 0.35f;
        };
        juce::Path p; const int steps = 48;
        for (int i = 0; i <= steps; ++i) {
            const float x = W * (float)i / (float)steps;
            if (i == 0) p.startNewSubPath(x, yTop(x)); else p.lineTo(x, yTop(x));
        }
        for (int i = steps; i >= 0; --i) {
            const float x = W * (float)i / (float)steps;
            p.lineTo(x, yTop(x) + thick * (0.8f + 0.4f * std::sin(x * 0.006f + t + (float)k)));
        }
        p.closeSubPath();
        const auto col = pick(f);
        juce::ColourGradient cg(col.withAlpha(0.f), 0.f, base - amp * 1.4f, col.withAlpha(0.f), 0.f, base + thick + amp * 1.4f, false);
        cg.addColour(0.4, col.withAlpha(juce::jlimit(0.f, 1.f, 0.55f * I)));
        g.setGradientFill(cg);
        g.fillPath(p);
    }
}

// ============================================================================
//  Starfield warp
// ============================================================================
void VfxRenderer::paintStarfield(juce::Graphics& g)
{
    const int v = var_ % 3;
    const float cx = W * 0.5f, cy = H * 0.5f, S = juce::jmin(W, H) * 0.6f * (0.5f + L_->scale);
    const float I = L_->intensity;
    for (auto& p : ps) {
        if (v == 1) {
            p.y += dt_ * (0.1f + p.aux * 0.25f) * (1.f + lvl_);
            p.x += std::sin(t + (float)p.id) * dt_ * 0.03f;
            if (p.y > 1.1f) { p.y = -1.1f; p.x = rng.nextFloat() * 2.f - 1.f; }
            blob(g, cx + p.x * W * 0.5f, cy + p.y * H * 0.5f, 2.f + p.aux * 4.f * (0.5f + L_->scale),
                 pick(p.u).withAlpha(juce::jlimit(0.f, 1.f, (0.4f + p.aux * 0.6f) * I)));
            continue;
        }
        p.aux -= dt_ * (0.35f + lvl_ * 0.9f);
        if (p.aux < 0.03f) { p.aux = 1.f; p.x = rng.nextFloat() * 2.f - 1.f; p.y = rng.nextFloat() * 2.f - 1.f; }
        const float ang = v == 2 ? t * 0.25f * (1.f - p.aux) : 0.f;
        const float ca = std::cos(ang), sa = std::sin(ang);
        const float rx = p.x * ca - p.y * sa, ry = p.x * sa + p.y * ca;
        const float sx = cx + rx / p.aux * S * 0.35f, sy = cy + ry / p.aux * S * 0.35f;
        const float sxp = cx + rx / (p.aux + 0.06f) * S * 0.35f, syp = cy + ry / (p.aux + 0.06f) * S * 0.35f;
        if (std::abs(sx - cx) > W * 0.6f || std::abs(sy - cy) > H * 0.6f) {
            p.aux = 1.f; p.x = rng.nextFloat() * 2.f - 1.f; p.y = rng.nextFloat() * 2.f - 1.f; continue;
        }
        g.setColour(pick(p.u).withAlpha(juce::jlimit(0.f, 1.f, std::pow(1.f - p.aux, 1.3f) * I)));
        g.drawLine(sxp, syp, sx, sy, 1.f + (1.f - p.aux) * 2.5f);
    }
}

// ============================================================================
//  Image / animated GIF / SVG
// ============================================================================
void VfxRenderer::paintMedia(juce::Graphics& g)
{
    const juce::String path = L_->path;
    if (path != loadedPath) {
        loadedPath = path; img = juce::Image(); gif = GifAnim(); svg.reset(); mediaMissing = false; gifIdx = 0; gifAcc = 0.f;
        const auto f = mvxResolve(path);
        if (f == juce::File()) mediaMissing = path.isNotEmpty();
        else {
            const auto ext = f.getFileExtension().toLowerCase();
            if (ext == ".gif") {
                if (!loadGif(f, gif)) { gif = GifAnim(); img = juce::ImageFileFormat::loadFrom(f); }
            } else if (ext == ".svg") svg = juce::Drawable::createFromImageFile(f);
            else img = juce::ImageFileFormat::loadFrom(f);
        }
    }
    juce::Image cur = img;
    if (gif.valid()) {
        gifAcc += dt_ * 1000.f;
        for (int guard = 0; guard < 64 && gifAcc >= (float)gif.delays[(size_t)gifIdx]; ++guard) {
            gifAcc -= (float)gif.delays[(size_t)gifIdx];
            gifIdx = (gifIdx + 1) % (int)gif.frames.size();
        }
        cur = gif.frames[(size_t)gifIdx];
    }
    const int fit = var_ % 4;
    if (cur.isValid()) {
        const float iw = (float)cur.getWidth(), ih = (float)cur.getHeight();
        if (fit == 3) {
            g.setTiledImageFill(cur, 0, 0, L_->intensity);
            g.fillRect(0.f, 0.f, W, H);
        } else {
            juce::Rectangle<float> dst(0.f, 0.f, W, H);
            if (fit != 2) {
                const float s = fit == 0 ? juce::jmax(W / iw, H / ih) : juce::jmin(W / iw, H / ih);
                dst = juce::Rectangle<float>(iw * s, ih * s).withCentre(juce::Point<float>(W * 0.5f, H * 0.5f));
            }
            g.setOpacity(L_->intensity);
            g.drawImageTransformed(cur, juce::AffineTransform::scale(dst.getWidth() / iw, dst.getHeight() / ih)
                                            .translated(dst.getX(), dst.getY()));
        }
    } else if (svg != nullptr) {
        svg->drawWithin(g, juce::Rectangle<float>(0.f, 0.f, W, H),
                        fit == 0 ? juce::RectanglePlacement::fillDestination
                                 : (fit == 2 ? juce::RectanglePlacement::stretchToFit : juce::RectanglePlacement::centred),
                        L_->intensity);
    } else {
        g.setColour(L_->c1.withAlpha(0.8f));
        g.setFont(16.f);
        g.drawText(mediaMissing ? "File not found: " + path : "Drop an image / GIF / SVG on the window, or press Browse (Skin tab)",
                   juce::Rectangle<float>(0.f, 0.f, W, H).toNearestInt(), juce::Justification::centred);
    }
}

// ============================================================================
//  Gradient
// ============================================================================
void VfxRenderer::paintGradient(juce::Graphics& g)
{
    const int v = var_ % 5;
    auto col = [&](int i) {
        if (L_->rainbow) return juce::Colour::fromHSV(std::fmod(t * 0.05f + (float)i * 0.2f, 1.f), 0.8f, 0.9f, 1.f);
        return i == 0 ? L_->c1 : (i == 1 ? L_->c2 : L_->c3);
    };
    const float a = juce::jlimit(0.f, 1.f, L_->intensity * (0.75f + 0.25f * lvl_));
    juce::ColourGradient cg;
    const float cx = W * 0.5f, cy = H * 0.5f;
    if (v == 0)      cg = juce::ColourGradient(col(0), 0.f, 0.f, col(2), 0.f, H, false);
    else if (v == 1) cg = juce::ColourGradient(col(0), 0.f, 0.f, col(2), W, H, false);
    else if (v == 2) cg = juce::ColourGradient(col(0), cx, cy, col(2), cx + juce::jmax(W, H) * 0.7f, cy, true);
    else if (v == 3) cg = juce::ColourGradient(col(0), cx, H, col(2), cx, H - H * 1.1f, true);
    else {
        const float ang = t * 0.3f, R = juce::jmax(W, H) * 0.6f;
        cg = juce::ColourGradient(col(0), cx + std::cos(ang) * R, cy + std::sin(ang) * R,
                                  col(2), cx - std::cos(ang) * R, cy - std::sin(ang) * R, false);
    }
    cg.addColour(0.5, col(1));
    for (int i = 0; i < cg.getNumColours(); ++i) cg.setColour(i, cg.getColour(i).withAlpha(a));
    g.setGradientFill(cg);
    g.fillRect(0.f, 0.f, W, H);
}
