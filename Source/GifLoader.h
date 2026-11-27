#pragma once
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <cstring>
#include <cstdint>

// Decodes every frame of an animated GIF (JUCE's own loader only reads frame 1).
struct GifAnim
{
    std::vector<juce::Image> frames;
    std::vector<int> delays;      // milliseconds per frame
    bool valid() const { return !frames.empty(); }
};

namespace gifdetail
{
    struct Reader
    {
        const uint8_t* d; size_t n; size_t p = 0;
        int u8()  { return p < n ? d[p++] : 0; }
        int u16() { const int a = u8(); const int b = u8(); return a | (b << 8); }
        void skipSubBlocks() { for (;;) { const int sz = u8(); if (sz == 0 || p >= n) break; p += (size_t)sz; } }
    };

    inline bool lzw(const std::vector<uint8_t>& in, int minCode, size_t outCount, std::vector<uint8_t>& out)
    {
        out.clear();
        if (minCode < 2 || minCode > 11) return false;
        const int clearCode = 1 << minCode, eoi = clearCode + 1;
        int codeSize = minCode + 1, next = eoi + 1;
        std::vector<uint16_t> prefix(4096, 0);
        std::vector<uint8_t> suffix(4096, 0), stack(4098);
        out.reserve(outCount);
        uint32_t bitBuf = 0; int bitCnt = 0; size_t ip = 0;
        int old = -1; uint8_t first = 0;
        while (out.size() < outCount)
        {
            while (bitCnt < codeSize) {
                if (ip >= in.size()) return !out.empty();
                bitBuf |= (uint32_t)in[ip++] << bitCnt; bitCnt += 8;
            }
            int code = (int)(bitBuf & ((1u << codeSize) - 1u));
            bitBuf >>= codeSize; bitCnt -= codeSize;
            if (code == clearCode) { codeSize = minCode + 1; next = eoi + 1; old = -1; continue; }
            if (code == eoi) break;
            if (old == -1) {
                if (code >= clearCode) return !out.empty();
                out.push_back((uint8_t)code); old = code; first = (uint8_t)code; continue;
            }
            const int inCode = code; int sp = 0;
            if (code >= next) { if (code > next) break; stack[(size_t)sp++] = first; code = old; }
            while (code >= clearCode) {
                if (sp >= 4096) return !out.empty();
                stack[(size_t)sp++] = suffix[(size_t)code]; code = prefix[(size_t)code];
            }
            first = (uint8_t)code; stack[(size_t)sp++] = first;
            while (sp > 0 && out.size() < outCount) out.push_back(stack[(size_t)--sp]);
            if (next < 4096) {
                prefix[(size_t)next] = (uint16_t)old; suffix[(size_t)next] = first; ++next;
                if (next == (1 << codeSize) && codeSize < 12) ++codeSize;
            }
            old = inCode;
        }
        return !out.empty();
    }
}

inline bool loadGif(const juce::File& file, GifAnim& out, int maxDim = 560, size_t maxBytes = 160u * 1024u * 1024u)
{
    out = GifAnim();
    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb) || mb.getSize() < 14) return false;
    gifdetail::Reader r{ (const uint8_t*)mb.getData(), mb.getSize() };
    if (std::memcmp(r.d, "GIF8", 4) != 0) return false;
    r.p = 6;
    const int W = r.u16(), H = r.u16();
    const int pk = r.u8(); r.u8(); r.u8();
    if (W <= 0 || H <= 0 || W > 8192 || H > 8192) return false;

    auto readPalette = [&](int bits, std::vector<uint32_t>& pal) {
        const int cnt = 1 << bits; pal.resize((size_t)cnt);
        for (int i = 0; i < cnt; ++i) {
            const int rr = r.u8(), gg = r.u8(), bb = r.u8();
            pal[(size_t)i] = 0xff000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
        }
    };
    std::vector<uint32_t> gct;
    if (pk & 0x80) readPalette((pk & 7) + 1, gct);

    std::vector<uint32_t> canvas((size_t)W * (size_t)H, 0u), backup;
    const float sc = juce::jmin(1.f, (float)maxDim / (float)juce::jmax(W, H));
    const int ow = juce::jmax(1, (int)(W * sc)), oh = juce::jmax(1, (int)(H * sc));

    int disposal = 0, transIdx = -1, delayMs = 100;
    int prevX = 0, prevY = 0, prevW = 0, prevH = 0, prevDisposal = 0;
    size_t bytesUsed = 0;

    while (r.p < r.n && out.frames.size() < 600)
    {
        const int b = r.u8();
        if (b == 0x3B) break;
        if (b == 0x21) {
            const int label = r.u8();
            if (label == 0xF9) {
                r.u8();                                   // block size (4)
                const int pf = r.u8();
                delayMs = r.u16() * 10;
                const int ti = r.u8();
                transIdx = (pf & 1) ? ti : -1;
                disposal = (pf >> 2) & 7;
                if (delayMs < 20) delayMs = 100;          // browsers do the same for 0/1 cs delays
            }
            r.skipSubBlocks();
            continue;
        }
        if (b != 0x2C) continue;

        const int fx = r.u16(), fy = r.u16(), fw = r.u16(), fh = r.u16();
        const int f = r.u8();
        std::vector<uint32_t> lct;
        if (f & 0x80) readPalette((f & 7) + 1, lct);
        const std::vector<uint32_t>& pal = (f & 0x80) ? lct : gct;
        const bool interlaced = (f & 0x40) != 0;
        const int minCode = r.u8();
        std::vector<uint8_t> data;
        for (;;) {
            const int sz = r.u8();
            if (sz == 0 || r.p >= r.n) break;
            const size_t take = juce::jmin((size_t)sz, r.n - r.p);
            data.insert(data.end(), r.d + r.p, r.d + r.p + take);
            r.p += take;
        }
        if (fw <= 0 || fh <= 0 || pal.empty()) continue;

        std::vector<uint8_t> idx;
        gifdetail::lzw(data, minCode, (size_t)fw * (size_t)fh, idx);
        idx.resize((size_t)fw * (size_t)fh, 0);

        // dispose the previous frame
        if (prevDisposal == 2) {
            for (int y = prevY; y < prevY + prevH && y < H; ++y)
                for (int x = prevX; x < prevX + prevW && x < W; ++x) canvas[(size_t)y * (size_t)W + (size_t)x] = 0u;
        } else if (prevDisposal == 3 && !backup.empty()) canvas = backup;
        if (disposal == 3) backup = canvas;

        // row order for interlaced frames
        std::vector<int> rowMap((size_t)fh);
        if (interlaced) {
            int row = 0;
            const int starts[4] = {0, 4, 2, 1}, steps[4] = {8, 8, 4, 2};
            for (int pass = 0; pass < 4; ++pass)
                for (int y = starts[pass]; y < fh; y += steps[pass]) rowMap[(size_t)row++] = y;
        } else for (int y = 0; y < fh; ++y) rowMap[(size_t)y] = y;

        for (int row = 0; row < fh; ++row) {
            const int cy = fy + rowMap[(size_t)row];
            if (cy < 0 || cy >= H) continue;
            for (int x = 0; x < fw; ++x) {
                const int cx = fx + x;
                if (cx < 0 || cx >= W) continue;
                const int ci = idx[(size_t)row * (size_t)fw + (size_t)x];
                if (ci == transIdx || ci >= (int)pal.size()) continue;
                canvas[(size_t)cy * (size_t)W + (size_t)cx] = pal[(size_t)ci];
            }
        }

        // snapshot
        bytesUsed += (size_t)ow * (size_t)oh * 4u;
        if (bytesUsed > maxBytes) break;
        juce::Image img(juce::Image::ARGB, ow, oh, true);
        {
            juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < oh; ++y) {
                const int sy = juce::jmin(H - 1, (int)((float)y / sc));
                for (int x = 0; x < ow; ++x) {
                    const int sx = juce::jmin(W - 1, (int)((float)x / sc));
                    bd.setPixelColour(x, y, juce::Colour((juce::uint32)canvas[(size_t)sy * (size_t)W + (size_t)sx]));
                }
            }
        }
        out.frames.push_back(img);
        out.delays.push_back(delayMs);

        prevX = fx; prevY = fy; prevW = fw; prevH = fh; prevDisposal = disposal;
        disposal = 0; transIdx = -1; delayMs = 100;
    }
    return out.valid();
}
