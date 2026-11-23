// We Become What We Behold -- native Wii U port.
// Behaviour follows the original JavaScript (Peep/World/Camera/Director/Act_I..III)
// tick for tick: the game runs on the same fixed 60 Hz update the browser build used.
#include <SDL.h>
#include <coreinit/debug.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "generated_sprites.h"

namespace {

using std::string;
using std::vector;

constexpr float W = 960.f, H = 540.f;
constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717958f;
constexpr float DRAWSCALE = 0.65f; // Peep.DRAWING_SCALE / AnimationProp.DRAWING_SCALE
constexpr float ZOOM_OUT_1_TIME = 2.0f;
constexpr float SEE_VIEWERS_TIME = 2.0f; // Director.SEE_VIEWERS_TIME = 1.5 + 0.5
constexpr float ZOOM_OUT_2_TIME = 2.0f;

inline int sec60(float seconds) { return int(std::ceil(seconds * 60.0f)); }

// Math.random() replacement (xorshift).
uint32_t g_rng = 0x6d2b79f5u;
inline uint32_t rnd32() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
inline float randf() { return float(rnd32() & 0xffffffu) * (1.0f / 16777216.0f); }
inline float randRange(float a, float b) { return a + randf() * (b - a); }

// ---------------------------------------------------------------- rendering
SDL_Renderer* g_rend = nullptr;
SDL_Texture* g_worldTex = nullptr;   // 960x540 world render target
SDL_Texture* g_sceneTex = nullptr;   // 960x540 scene render target

struct Col { Uint8 r = 255, g = 255, b = 255, a = 255; };
const Col WHITE{255, 255, 255, 255};
const Col BLACK{0, 0, 0, 255};

// World-space -> texture-space transform (World.graphics pivot/scale).
struct View { float s = 1.f, tx = 0.f, ty = 0.f; };
View g_view;

inline void applyView(float x, float y, float& ox, float& oy) {
    ox = x * g_view.s + g_view.tx;
    oy = y * g_view.s + g_view.ty;
}

void drawTex(SDL_Texture* t, const SDL_Rect* src, float cx, float cy, float w, float h,
             float angle = 0.f, bool flip = false, float alpha = 1.f) {
    if (!t || alpha <= 0.002f) return;
    float px, py;
    applyView(cx, cy, px, py);
    const float dw = w * g_view.s, dh = h * g_view.s;
    // The TV cut scales the whole world by 8x, so most sprites end up far outside
    // the 960x540 target while the backend still sets up (and the GPU discards)
    // every one of those quads. Drop the ones that cannot touch the canvas; 0.75
    // times the larger side covers the bounding circle of any rotation.
    const float reach = std::max(std::fabs(dw), std::fabs(dh)) * 0.75f;
    if (px + reach < 0.f || px - reach > W || py + reach < 0.f || py - reach > H) return;
    SDL_Rect dst{int(std::lround(px - dw * .5f)), int(std::lround(py - dh * .5f)),
                 int(std::lround(dw)), int(std::lround(dh))};
    if (dst.w < 1) dst.w = 1;
    if (dst.h < 1) dst.h = 1;
    SDL_SetTextureAlphaMod(t, Uint8(std::clamp(alpha, 0.f, 1.f) * 255.f));
    SDL_RenderCopyEx(g_rend, t, src, &dst, double(angle), nullptr,
                     flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(t, 255);
}

void fillRect(float x, float y, float w, float h, Col c) {
    float px, py;
    applyView(x, y, px, py);
    SDL_SetRenderDrawColor(g_rend, c.r, c.g, c.b, c.a);
    SDL_Rect r{int(std::lround(px)), int(std::lround(py)),
               int(std::lround(w * g_view.s)), int(std::lround(h * g_view.s))};
    if (r.w < 1) r.w = 1;
    if (r.h < 1) r.h = 1;
    SDL_RenderFillRect(g_rend, &r);
}

// ---------------------------------------------------------------- pixel font
// 5x7 glyphs, horizontally condensed so line widths stay close to Cairo's.
const uint8_t* glyph(char c) {
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t A[26][7] = {
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}};
    static const uint8_t a[26][7] = {
        {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},{0x10,0x10,0x16,0x19,0x11,0x11,0x1E},
        {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E},{0x01,0x01,0x0D,0x13,0x11,0x11,0x0F},
        {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},{0x06,0x09,0x08,0x1C,0x08,0x08,0x08},
        {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E},{0x10,0x10,0x16,0x19,0x11,0x11,0x11},
        {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},{0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
        {0x10,0x10,0x12,0x14,0x18,0x14,0x12},{0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x00,0x00,0x1A,0x15,0x15,0x11,0x11},{0x00,0x00,0x16,0x19,0x11,0x11,0x11},
        {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},{0x00,0x1E,0x11,0x11,0x1E,0x10,0x10},
        {0x00,0x0D,0x13,0x13,0x0D,0x01,0x01},{0x00,0x00,0x16,0x19,0x10,0x10,0x10},
        {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E},{0x08,0x08,0x1C,0x08,0x08,0x09,0x06},
        {0x00,0x00,0x11,0x11,0x11,0x13,0x0D},{0x00,0x00,0x11,0x11,0x11,0x0A,0x04},
        {0x00,0x00,0x11,0x11,0x15,0x15,0x0A},{0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
        {0x00,0x11,0x11,0x11,0x0F,0x01,0x0E},{0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}};
    static const uint8_t D[10][7] = {
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}};
    static const uint8_t dot[7] = {0,0,0,0,0,0x0C,0x0C};
    static const uint8_t comma[7] = {0,0,0,0,0x0C,0x0C,0x08};
    static const uint8_t bang[7] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04};
    static const uint8_t quest[7] = {0x0E,0x11,0x01,0x02,0x04,0x00,0x04};
    static const uint8_t quote[7] = {0x04,0x04,0,0,0,0,0};
    static const uint8_t lp[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02};
    static const uint8_t rp[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08};
    static const uint8_t star[7] = {0,0x04,0x15,0x0E,0x15,0x04,0};
    static const uint8_t colon[7] = {0,0x0C,0x0C,0,0x0C,0x0C,0};
    static const uint8_t dash[7] = {0,0,0,0x1F,0,0,0};
    static const uint8_t lt[7] = {0x02,0x04,0x08,0x10,0x08,0x04,0x02};
    static const uint8_t gt[7] = {0x08,0x04,0x02,0x01,0x02,0x04,0x08};
    static const uint8_t slash[7] = {0x01,0x01,0x02,0x04,0x08,0x10,0x10};
    static const uint8_t plus[7] = {0,0x04,0x04,0x1F,0x04,0x04,0};
    if (c >= 'A' && c <= 'Z') return A[c - 'A'];
    if (c >= 'a' && c <= 'z') return a[c - 'a'];
    if (c >= '0' && c <= '9') return D[c - '0'];
    switch (c) {
        case '.': return dot;
        case ',': return comma;
        case '!': return bang;
        case '?': return quest;
        case '\'': return quote;
        case '(': return lp;
        case ')': return rp;
        case '*': return star;
        case ':': return colon;
        case '-': return dash;
        case '<': return lt;
        case '>': return gt;
        case '/': return slash;
        case '+': return plus;
        default: return blank;
    }
}

constexpr float FONT_CONDENSE = 0.6f; // horizontal squeeze of the 5x7 font

float textLine(const string& s, float x, float y, float pxHeight, Col c) {
    float sc = pxHeight / 7.f;
    float gw = sc * FONT_CONDENSE, gh = sc;
    float adv = gw * 6.f;
    for (char ch : s) {
        const uint8_t* g = glyph(ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (g[row] & (1 << (4 - col)))
                    fillRect(x + col * gw, y + row * gh, gw + 0.2f, gh + 0.2f, c);
        x += adv;
    }
    return s.size() * adv;
}

float textWidth(const string& s, float pxHeight) {
    return s.size() * 6.f * (pxHeight / 7.f) * FONT_CONDENSE;
}

void textCentered(const string& s, float cx, float y, float pxHeight, Col c) {
    size_t from = 0;
    float lineY = y;
    while (from <= s.size()) {
        size_t to = s.find('\n', from);
        if (to == string::npos) to = s.size();
        string line = s.substr(from, to - from);
        textLine(line, cx - textWidth(line, pxHeight) * .5f, lineY, pxHeight, c);
        lineY += pxHeight * 1.25f;
        if (to == s.size()) break;
        from = to + 1;
    }
}

// ---------------------------------------------------------------- tweens
enum class Ease { Linear, QuadInOut, CircInOut, CubicInOut };

float easeApply(Ease e, float t) {
    t = std::clamp(t, 0.f, 1.f);
    switch (e) {
        case Ease::QuadInOut: return t < .5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
        case Ease::CircInOut:
            return t < .5f ? -0.5f * (std::sqrt(1 - 4 * t * t) - 1)
                           : 0.5f * (std::sqrt(1 - std::pow(-2 * t + 2, 2.f)) + 1);
        case Ease::CubicInOut: return t < .5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.f) * .5f;
        default: return t;
    }
}

struct TweenOp {
    enum Kind { Wait, To, Call } kind = Wait;
    int ticks = 0, elapsed = 0;
    std::function<float()> get;
    std::function<void(float)> set;
    float target = 0.f, start = 0.f;
    Ease ease = Ease::Linear;
    std::function<void()> call;
};

struct Tween {
    vector<TweenOp> ops;
    size_t idx = 0;
    bool active = false, finished = false;

    void play() { idx = 0; active = true; finished = false; }
    void update() {
        if (!active || finished) return;
        if (idx >= ops.size()) { active = false; finished = true; return; }
        TweenOp& op = ops[idx];
        if (op.kind == TweenOp::Wait) {
            if (++op.elapsed >= op.ticks) ++idx;
        } else if (op.kind == TweenOp::Call) {
            auto f = op.call;
            ++idx;
            if (f) f();
        } else {
            if (op.elapsed == 0 && op.get) op.start = op.get();
            ++op.elapsed;
            float p = op.ticks > 0 ? float(op.elapsed) / float(op.ticks) : 1.f;
            if (op.set) op.set(op.start + (op.target - op.start) * easeApply(op.ease, p));
            if (op.elapsed >= op.ticks) ++idx;
        }
        if (idx >= ops.size()) { active = false; finished = true; }
    }
};

void twWait(Tween& t, int ticks) {
    TweenOp o; o.kind = TweenOp::Wait; o.ticks = ticks; t.ops.push_back(o);
}
void twTo(Tween& t, int ticks, std::function<float()> get, float target,
          std::function<void(float)> set, Ease e = Ease::Linear) {
    TweenOp o; o.kind = TweenOp::To; o.ticks = ticks; o.get = get; o.set = set;
    o.target = target; o.ease = e; t.ops.push_back(o);
}
void twCall(Tween& t, std::function<void()> f) {
    TweenOp o; o.kind = TweenOp::Call; o.call = f; t.ops.push_back(o);
}

// ---------------------------------------------------------------- assets
std::map<string, SDL_Texture*> g_textures;

SDL_Texture* T(const char* name) {
    auto it = g_textures.find(name);
    return it == g_textures.end() ? nullptr : it->second;
}

SDL_Texture* loadBmp(const char* name) {
    const string paths[] = {string("/vol/content/") + name, string("romfs:/assets/") + name,
                            string("assets/") + name, string("../wiiu/assets/") + name};
    SDL_Surface* s = nullptr;
    for (const string& p : paths) {
        s = SDL_LoadBMP(p.c_str());
        if (s) break;
    }
    if (!s) { SDL_Log("asset load failed: %s (%s)", name, SDL_GetError()); return nullptr; }
    if (s->format && s->format->palette) {
        for (int i = 0; i < s->format->palette->ncolors; ++i) {
            const SDL_Color& c = s->format->palette->colors[i];
            if (c.r == 255 && c.g == 0 && c.b == 255) { SDL_SetColorKey(s, SDL_TRUE, Uint32(i)); break; }
        }
    } else {
        SDL_SetColorKey(s, SDL_TRUE, SDL_MapRGB(s->format, 255, 0, 255));
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_rend, s);
    SDL_FreeSurface(s);
    // SDL gives a texture with no alpha channel and no colour key BLENDMODE_NONE,
    // which makes SDL_SetTextureAlphaMod a no-op: the grain overlay (snow) then
    // covers the whole game instead of sitting on it at 0.15 alpha. Every asset
    // here is drawn with the renderer's blending, so force it on for all of them.
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

void loadArt() {
    static const char* names[] = {
        "bg","bg_dark","bg_shade","tv","cam","cam-flash","cam-instructions",
        "body","body_red","face","face_murder","hat","hangry","hatguy","gun",
        "peace","helping","face_nervous","face_snobby","face_snobby_hmph",
        "face_angry","happy_weirdo","lovehat","lover_shirt","lover_panic",
        "gore","gore_bodies","blood","snow","laptop",
        "chyron","chyron2","chyron3","cricket","candlelight","lovers_watching",
        "cursor","weapon_gun","weapon_axe","weapon_bat","weapon_shotgun",
        "quote0001","quote0002","quote0003","quote0004",
        "credits0001","credits0002","credits0003","credits0004",
        "credits0005","credits0006","credits0007","credits0008",
        "logo","end_button","facebook","twitter","portbrew"};
    for (const char* n : names) {
        string file = string(n) + ".bmp";
        g_textures[n] = loadBmp(file.c_str());
    }
}

// ---------------------------------------------------------------- audio
// devkitPro's portlibs have no SDL2_mixer, so this is a small software mixer on
// top of SDL's audio device. WAVs are converted to the device format at load.
struct AudioVoice {
    const vector<Uint8>* pcm = nullptr;
    size_t pos = 0;
    bool loop = false, active = false;
    float vol = 1.f, fadeFrom = 1.f, fadeTo = 1.f;
    int fadeTicks = 0, fadeTotal = 0;
};

struct AudioEngine {
    bool ready = false;
    SDL_AudioDeviceID dev = 0;
    SDL_AudioSpec have{};
    std::map<string, vector<Uint8>> clips;
    AudioVoice voices[32];
    vector<int32_t> mix;
};
AudioEngine g_audio;
int g_parkVoice = -1, g_panicVoice = -1, g_creepyVoice = -1, g_nightVoice = -1;

// Our own voice lock instead of SDL_LockAudioDevice: the main thread can then
// never be left waiting on SDL's audio device lock if the audio thread is
// delayed, and the audio thread never blocks (it just outputs silence for a
// buffer while the game is editing voices).
std::atomic_flag g_audioBusy = ATOMIC_FLAG_INIT;
// Bounded: the game never waits on the audio thread. If the audio side is late or
// wedged, the sound for that moment is dropped instead of stalling the frame.
inline bool audioLock() {
    for (int i = 0; i < 1000; ++i) {
        if (!g_audioBusy.test_and_set(std::memory_order_acquire)) return true;
    }
    return false;
}
inline void audioUnlock() { g_audioBusy.clear(std::memory_order_release); }

void audioMixCallback(void*, Uint8* stream, int len) {
    SDL_memset(stream, 0, len);
    AudioEngine& a = g_audio;
    if (!a.ready) return;
    int ch = int(a.have.channels);
    int frames = len / (2 * ch);
    if (frames <= 0) return;
    if (g_audioBusy.test_and_set(std::memory_order_acquire)) return; // game is editing voices
    if (size_t(frames) > a.mix.size()) { audioUnlock(); return; } // never allocate here
    std::fill(a.mix.begin(), a.mix.begin() + frames, 0);
    for (AudioVoice& v : a.voices) {
        if (!v.active || !v.pcm || v.pcm->size() < 2) continue;
        const int16_t* src = reinterpret_cast<const int16_t*>(v.pcm->data());
        size_t totalFrames = v.pcm->size() / (size_t(2) * ch);
        if (totalFrames == 0) { v.active = false; continue; }
        for (int i = 0; i < frames; ++i) {
            if (v.pos >= totalFrames) {
                if (v.loop) v.pos = 0;
                else { v.active = false; break; }
            }
            int32_t s = 0;
            for (int c = 0; c < ch; ++c) s += src[v.pos * ch + c];
            a.mix[i] += int32_t(float(s / ch) * v.vol);
            ++v.pos;
        }
    }
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    for (int i = 0; i < frames; ++i) {
        int32_t m = std::clamp(a.mix[i], -32768, 32767);
        for (int c = 0; c < ch; ++c) out[i * ch + c] = int16_t(m);
    }
    audioUnlock();
}

void audioLoadClip(const char* name) {
    if (!g_audio.ready) return;
    static const char* dirs[] = {"/vol/content/audio/", "assets/audio/", "../wiiu/assets/audio/"};
    SDL_AudioSpec spec{};
    Uint8* buf = nullptr;
    Uint32 len = 0;
    bool ok = false;
    for (const char* d : dirs) {
        string path = string(d) + name + ".wav";
        if (SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) { ok = true; break; }
    }
    if (!ok) { SDL_Log("audio load failed: %s", name); return; }
    SDL_AudioCVT cvt{};
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, g_audio.have.format,
                          g_audio.have.channels, g_audio.have.freq) < 0) {
        SDL_FreeWAV(buf);
        return;
    }
    cvt.len = int(len);
    cvt.buf = (Uint8*)SDL_malloc(size_t(len) * size_t(cvt.len_mult));
    if (!cvt.buf) { SDL_FreeWAV(buf); return; }
    SDL_memcpy(cvt.buf, buf, len);
    SDL_FreeWAV(buf);
    if (SDL_ConvertAudio(&cvt) < 0) { SDL_free(cvt.buf); return; }
    g_audio.clips[name].assign(cvt.buf, cvt.buf + cvt.len_cvt);
    SDL_free(cvt.buf);
}

void audioInit() {
    static const char* names[] = {
        "bg_park", "bg_panic", "bg_creepy", "bg_nighttime", "breaking_news", "cam_snap",
        "crickets", "gun_cock", "gunshot", "impact", "peep_gasp", "peep_hmph", "peep_hngh",
        "peep_huh", "scream", "shotgun", "shout", "squeak"};
    SDL_AudioSpec want{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audioMixCallback;
    g_audio.dev = SDL_OpenAudioDevice(nullptr, 0, &want, &g_audio.have, 0);
    if (!g_audio.dev) { SDL_Log("audio device failed: %s", SDL_GetError()); return; }
    g_audio.ready = true;
    // Pre-size the mix buffer: the audio callback must never allocate (a heap
    // lock shared with the game thread is a potential deadlock on consoles).
    g_audio.mix.assign(16384, 0);
    for (const char* n : names) audioLoadClip(n);
    SDL_PauseAudioDevice(g_audio.dev, 0);
}

int audioPlay(const char* name, bool loop, float vol) {
    if (!g_audio.ready) return -1;
    auto it = g_audio.clips.find(name);
    if (it == g_audio.clips.end() || it->second.empty()) return -1;
    if (!audioLock()) return -1;
    int slot = -1;
    for (int i = 0; i < 32; ++i) {
        if (g_audio.voices[i].active && g_audio.voices[i].pcm == &it->second) { slot = i; break; }
        if (!g_audio.voices[i].active && slot < 0) slot = i;
    }
    if (slot < 0) slot = 0;
    AudioVoice& v = g_audio.voices[slot];
    v.pcm = &it->second;
    v.pos = 0;
    v.loop = loop;
    v.active = true;
    v.vol = v.fadeFrom = v.fadeTo = std::clamp(vol, 0.f, 2.f);
    v.fadeTicks = v.fadeTotal = 0;
    audioUnlock();
    return slot;
}

void audioStop(int id, float fadeSec = 0.f) {
    if (!g_audio.ready || id < 0 || id >= 32) return;
    audioLock();
    AudioVoice& v = g_audio.voices[id];
    if (!v.active) { audioUnlock(); return; }
    if (fadeSec <= 0.f) {
        v.active = false;
        v.fadeTicks = v.fadeTotal = 0;
    } else {
        v.fadeFrom = v.vol;
        v.fadeTo = 0.f;
        v.fadeTotal = v.fadeTicks = sec60(fadeSec);
    }
    audioUnlock();
}

void audioSetVolume(int id, float target, float fadeSec = 0.f) {
    if (!g_audio.ready || id < 0 || id >= 32) return;
    audioLock();
    AudioVoice& v = g_audio.voices[id];
    if (!v.active) { audioUnlock(); return; }
    if (fadeSec <= 0.f) {
        v.vol = v.fadeFrom = v.fadeTo = std::clamp(target, 0.f, 2.f);
        v.fadeTicks = v.fadeTotal = 0;
    } else {
        v.fadeFrom = v.vol;
        v.fadeTo = std::clamp(target, 0.f, 2.f);
        v.fadeTotal = v.fadeTicks = sec60(fadeSec);
    }
    audioUnlock();
}

void audioStopAll(float fadeSec = 0.f) {
    for (int i = 0; i < 32; ++i) audioStop(i, fadeSec);
}

void audioTick() {
    if (!g_audio.ready) return;
    if (!audioLock()) return;
    for (AudioVoice& v : g_audio.voices) {
        if (!v.active || v.fadeTicks <= 0) continue;
        --v.fadeTicks;
        float t = 1.f - float(v.fadeTicks) / float(std::max(1, v.fadeTotal));
        v.vol = v.fadeFrom + (v.fadeTo - v.fadeFrom) * t;
        if (v.fadeTicks <= 0) {
            v.vol = v.fadeTo;
            if (v.fadeTo <= 0.f) v.active = false;
        }
    }
    audioUnlock();
}

// ---------------------------------------------------------------- stall tracer
// Small ring of checkpoints plus a watchdog thread: if the game ever stops making
// progress, the last checkpoints are written to a file so the hang can be located
// without logging every frame.
std::atomic<unsigned> g_beat{0};
std::atomic<int> g_ckptIdx{0};
char g_ckptRing[24][48];

void ckpt(const char* s) {
    int i = g_ckptIdx.fetch_add(1) % 24;
    std::snprintf(g_ckptRing[i], sizeof(g_ckptRing[i]), "%s", s);
    g_beat.fetch_add(1, std::memory_order_relaxed);
}

void writeStallFile(const char* path) {
    SDL_RWops* rw = SDL_RWFromFile(path, "wb");
    if (!rw) return;
    char line[64];
    for (int k = 23; k >= 0; --k) {
        int i = (g_ckptIdx.load() - 1 - k + 48) % 24;
        std::snprintf(line, sizeof(line), "%s\n", g_ckptRing[i]);
        SDL_RWwrite(rw, line, 1, std::strlen(line));
    }
    SDL_RWclose(rw);
}

int stallWatch(void*) {
    unsigned last = g_beat.load();
    int idle = 0;
    for (;;) {
        SDL_Delay(200);
        unsigned b = g_beat.load();
        if (b != last) { last = b; idle = 0; continue; }
        ++idle;
        if (idle == 15) { // ~3 seconds with no progress: dump the ring to a file
            char* pref = SDL_GetPrefPath("wbwwb", "wbwwb");
            if (pref) {
                string p = string(pref) + "stall.txt";
                SDL_free(pref);
                writeStallFile(p.c_str());
            }
            // Several candidate roots: the app CWD and the usual Wii U SD mount.
            static const char* paths[] = {"wbwwb_stall.txt", "sd:/wbwwb_stall.txt",
                                          "fs:/vol/external01/wbwwb_stall.txt",
                                          "/vol/external01/wbwwb_stall.txt"};
            for (const char* p : paths) writeStallFile(p);
            SDL_Log("wbwwb: no progress for 3s, checkpoints in wbwwb_stall.txt");
        }
        // Still nothing after ~20 s: this is not a slow frame. Put the last steps
        // on the TV itself, so the freeze point can be read without taking the SD
        // card out (a hard hang leaves no other channel).
        if (idle == 100) {
            string msg = "wbwwb stopped responding for 20 s.\nLast steps (oldest first):\n";
            int n = g_ckptIdx.load();
            int count = n < 10 ? n : 10;
            for (int k = count - 1; k >= 0; --k) {
                int i = (n - 1 - k + 48) % 24;
                if (g_ckptRing[i][0]) { msg += g_ckptRing[i]; msg += "\n"; }
            }
            msg += "(same list is in sd:/wbwwb_stall.txt)";
            OSFatal(msg.c_str());
            for (;;) SDL_Delay(1000);
        }
    }
    return 0;
}

// ---------------------------------------------------------------- actors
struct World;
struct SceneCtx;
struct Director;
struct Peep;
struct GameScene;
struct ProtestAnim;

struct Actor {
    virtual ~Actor() {}
    const char* cls = "Actor";
    World* world = nullptr;
    float x = 0, y = 0, z = 0, width = 0, height = 0;
    bool dead = false;
    virtual void update() {}
    virtual void draw() {}
    void kill();
};

struct Spot { float x = 0, y = 0, radius = 0; };

struct World {
    SceneCtx* ctx = nullptr;
    string bgName = "bg";
    float pivotX = 480.f, pivotY = 270.f, viewScale = 1.f; // World.graphics pivot/scale
    // Post-credits darkening (bg_shade in the original: black at a soft per-pixel
    // alpha). A colour key cannot carry partial alpha, so it is drawn as one
    // uniform wash at the source image's mean alpha instead of a vignette.
    float shadeAlpha = 0.f;
    vector<std::unique_ptr<Actor>> arena;
    vector<Actor*> props, peeps, bg;

    void init(SceneCtx* c, const char* background) { ctx = c; bgName = background; }

    template <class Ty, class... A> Ty* make(A&&... a) {
        auto p = std::make_unique<Ty>(std::forward<A>(a)...);
        Ty* raw = p.get();
        arena.push_back(std::move(p));
        return raw;
    }
    void addProp(Actor* a) {
        if (!a) return;
        a->world = this;
        props.push_back(a);
    }
    void addPeep(Actor* p) {
        if (!p) return;
        peeps.push_back(p);
        addProp(p);
    }
    void addBG(Actor* a) {
        if (!a) return;
        a->world = this;
        bg.push_back(a);
    }
    void remove(Actor* a) {
        props.erase(std::remove(props.begin(), props.end(), a), props.end());
        peeps.erase(std::remove(peeps.begin(), peeps.end(), a), peeps.end());
        bg.erase(std::remove(bg.begin(), bg.end(), a), bg.end());
    }
    void clearPeeps() {
        vector<Actor*> copy = peeps;
        for (Actor* p : copy) p->kill();
    }
    void addBalancedPeeps(int howMany); // defined with the peep classes

    void update() {
        for (Actor* a : bg) a->update();
        vector<Actor*> snapshot = props;
        for (Actor* a : snapshot) if (!a->dead) a->update();
        std::stable_sort(props.begin(), props.end(), [](Actor* a, Actor* b) { return a->y < b->y; });
    }
    void draw() {
        drawTex(T(bgName.c_str()), nullptr, 480.f, 270.f, 1160.f, 740.f);
        if (shadeAlpha > 0.f)
            fillRect(0.f, 0.f, W, H, Col{0, 0, 0,
                Uint8(std::clamp(shadeAlpha, 0.f, 1.f) * 255.f)});
        for (Actor* a : bg) a->draw();
        ckpt("w.bg");
        for (Actor* a : props) a->draw();
        ckpt("w.props");
    }
};

void Actor::kill() {
    dead = true;
    if (world) world->remove(this);
}

struct SceneCtx {
    World* world = nullptr;
    struct TV* tv = nullptr;
    struct Camera* camera = nullptr;
    Director* director = nullptr;
    struct ScreenShake* shaker = nullptr;
    vector<Spot> avoidSpots;
    bool noYellingYet = false;
};

// ---------------------------------------------------------------- TV (TV.js)
struct TV : Actor {
    SDL_Texture* photo = nullptr;
    SDL_Rect photoSrc{0, 0, 240, 135};
    bool hasPhoto = false, fail = false, nothing = false;
    string headline;
    Tween chyronTween;
    float chyronAlpha = 0.f, chyronX = 15.f;

    TV() { cls = "TV"; x = 480; y = 350; width = 150; height = 180; }

    void placePhoto(SDL_Texture* tex, const string& text, bool isFail, bool isNothing,
                    const SDL_Rect& src = SDL_Rect{0, 0, 0, 0}) {
        photo = tex; hasPhoto = true; fail = isFail; nothing = isNothing; headline = text;
        if (src.w > 0) photoSrc = src;
        chyronTween = Tween(); chyronAlpha = 0.f; chyronX = 15.f;
        twTo(chyronTween, sec60(0.5f), [this] { return chyronAlpha; }, 1.f,
             [this](float v) { chyronAlpha = v; }, Ease::QuadInOut);
        twTo(chyronTween, sec60(0.8f), [this] { return chyronX; }, 0.f,
             [this](float v) { chyronX = v; }, Ease::QuadInOut);
        chyronTween.play();
    }

    void update() override { chyronTween.update(); }

    void draw() override {
        // bg anchor (0.5, 1), scale 0.5 -> 150 x 180 at (x, y)
        drawTex(T("tv"), nullptr, x, y - 90.f, 150.f, 180.f);
        const float px = 420.f, py = 202.75f, pw = 120.f, ph = 67.5f;
        if (hasPhoto && photo) drawTex(photo, &photoSrc, px + pw * .5f, py + ph * .5f, pw, ph);
        const char* banner = nothing ? "chyron3" : (fail ? "chyron2" : "chyron");
        drawTex(T(banner), nullptr, px + pw * .5f + chyronX * .5f, py + ph * .5f, pw, ph,
                0.f, false, chyronAlpha);
        if (!nothing && !headline.empty()) {
            textLine(headline, px + 15.f + chyronX * .5f, py + 57.5f, 5.f,
                     Col{255, 255, 255, Uint8(std::clamp(chyronAlpha, 0.f, 1.f) * 255.f)});
        }
    }
};

// ---------------------------------------------------------------- Camera (Camera.js)
struct Camera {
    SceneCtx* ctx = nullptr;
    float x = 480, y = 270, width = 240, height = 135, scale = 1.f;
    bool frozen = false, visible = true, noIntro = false, streaming = false, hasPhoto = false;
    bool noSounds = false;
    float flashAlpha = 0.f, instrAlpha = 0.f;
    Tween flashTween, instrTween;
    SDL_Texture* photoTexture = nullptr;
    SDL_Rect photoSrc{0, 0, 240, 135};
    SDL_Texture* pool[2] = {nullptr, nullptr};
    int poolIndex = 0;
    std::function<void()> onTakePhoto;

    void init(SceneCtx* c) {
        ctx = c;
        // Snapshot at full scene size so no render-target scaling can creep in;
        // the 240x135 framing is kept as a source rect (photoSrc).
        pool[0] = SDL_CreateTexture(g_rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, int(W), int(H));
        pool[1] = SDL_CreateTexture(g_rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, int(W), int(H));
        if (!noIntro) {
            instrTween = Tween();
            twWait(instrTween, sec60(1.f));
            twTo(instrTween, sec60(0.5f), [this] { return instrAlpha; }, 1.f,
                 [this](float v) { instrAlpha = v; });
            instrTween.play();
        }
    }

    void updatePosition() {
        if (x < width * .5f) x = width * .5f;
        if (x > W - width * .5f) x = W - width * .5f;
        if (y < height * .5f) y = height * .5f;
        if (y > H - height * .5f) y = H - height * .5f;
    }

    void update() {
        if (!frozen) updatePosition();
        flashTween.update();
        instrTween.update();
    }

    SDL_Texture* getTexture() {
        SDL_Texture* rt = pool[poolIndex];
        poolIndex = (poolIndex + 1) % 2;
        if (!rt || !g_worldTex) return nullptr;
        SDL_SetRenderTarget(g_rend, rt);
        SDL_Rect full{0, 0, int(W), int(H)};
        SDL_RenderSetViewport(g_rend, &full);
        SDL_RenderSetScale(g_rend, 1.f, 1.f);
        SDL_SetRenderDrawColor(g_rend, 0, 0, 0, 255);
        SDL_RenderClear(g_rend);
        SDL_RenderCopy(g_rend, g_worldTex, nullptr, &full);
        // Setting the target back to null restores the logical viewport that was
        // saved when the first target was set; no renderer reset is needed here
        // (and resetting mid-event was a needless backend state change).
        SDL_SetRenderTarget(g_rend, nullptr);
        // The photo frame is a crop of that full-size snapshot.
        photoSrc = SDL_Rect{int(std::lround(x - width * .5f)), int(std::lround(y - height * .5f)),
                            int(width), int(height)};
        return rt;
    }

    void takePhoto() {
        if (!streaming) {
            updatePosition();
            photoTexture = getTexture();
            hasPhoto = true;
        }
        if (!noSounds) audioPlay("cam_snap", false, 1.f);
        flashAlpha = 1.f;
        flashTween = Tween();
        twTo(flashTween, sec60(0.25f), [this] { return flashAlpha; }, 0.f,
             [this](float v) { flashAlpha = v; });
        flashTween.play();
        if (!noIntro && instrAlpha > 0.f) {
            instrTween = Tween();
            twTo(instrTween, sec60(0.25f), [this] { return instrAlpha; }, 0.f,
                 [this](float v) { instrAlpha = v; });
            instrTween.play();
        }
        if (onTakePhoto) onTakePhoto();
    }

    void hide() { visible = false; hasPhoto = false; }

    void reset() {
        scale = 1.f; visible = true; frozen = false; hasPhoto = false; updatePosition();
    }

    void draw() {
        if (!visible) return;
        if (hasPhoto && photoTexture) drawTex(photoTexture, &photoSrc, x, y, width * scale, height * scale);
        if (flashAlpha > 0.f) drawTex(T("cam-flash"), nullptr, x, y, width * scale, height * scale, 0, false, flashAlpha);
        drawTex(T("cam"), nullptr, x, y, width * scale, height * scale);
        if (!noIntro && instrAlpha > 0.f)
            drawTex(T("cam-instructions"), nullptr, x, y + 67.5f * scale, 240.f * scale, 50.f * scale, 0, false, instrAlpha);
    }

    bool isOverTV(bool smaller) {
        float w = smaller ? 140.f : width;
        float h = smaller ? 90.f : height;
        return (W * .5f - w * .5f < x && x < W * .5f + w * .5f &&
                H * .5f - h * .5f < y && y < H * .5f + h * .5f);
    }
};

// ---------------------------------------------------------------- Director (Director.js)
struct PhotoData {
    int audience = 0, audienceCircles = 0, audienceSquares = 0;
    bool HIJACK = false, forceChyron = false, ITS_NOTHING = false, CAUGHT_A_CRICKET = false;
    bool caughtHat = false, caughtLovers = false, caughtCrazy = false;
    bool caughtNervous = false, caughtSnobby = false;
    bool caughtAngry = false, caughtAngryCircle = false, caughtAngrySquare = false;
    Actor* caughtHatProp = nullptr;
    Actor* caughtCrazyProp = nullptr;
    Actor* caughtNervousProp = nullptr;
    Actor* caughtSnobbyProp = nullptr;
};

struct Director {
    SceneCtx* ctx = nullptr;
    PhotoData data;
    bool isWatchingTV = false;
    bool noSounds = false;
    string chyron = "[NO CHYRON]";
    SDL_Texture* photoTexture = nullptr;
    SDL_Rect photoSrc{0, 0, 240, 135};
    TV* tv = nullptr;

    Tween mainTween, camXTween, camYTween, camScaleTween, viewTween;
    float viewT = 0.f;
    float vsl = 0, vsr = 0, vst = 0, vsb = 0, vel = 0, ver = 0, vet = 0, veb = 0;
    bool viewTweening = false;

    std::function<void(Director&)> cbTakePhoto, cbMovePhoto, cbCutToTV, cbReset;

    void init(SceneCtx* c) { ctx = c; }
    void takePhoto(Camera* camera);
    void update();
    void cutViewportTo(float vx, float vy, float s);
    void tweenViewportTo(float vx, float vy, float s, int ticks, Ease e);
    vector<Actor*> inCamera(std::function<bool(Actor*)> filter);
    bool audienceCutToTV(std::function<void(Peep*)> doElse = {}, std::function<bool(Peep*)> filterAudience = {});
    void audienceMovePhoto() {
        if (!ctx) return;
        if (noSounds) return;
        if (data.audience > 0 || data.audienceCircles > 0 || data.audienceSquares > 0 || data.forceChyron)
            audioPlay("breaking_news", false, 1.f);
    }
};

// ---------------------------------------------------------------- Peep (Peep.js)
struct AnimCall { std::function<void()> fn; int ticks = 0; };

struct Peep : Actor {
    SceneCtx* ctx = nullptr;
    float hop = 0, lastHop = 0, speed = 1, direction = 0, velX = 0, velY = 0;
    float flip = 1, bounce = 1, bounceVel = 0;
    float rotation = 0, pivotY = 0;
    bool isWalking = true, loop = true, goThroughSpots = false, isWatching = false;
    bool pauseAnimsWhenNotWatching = false;
    vector<AnimCall> animCalls;

    explicit Peep(SceneCtx* c) : ctx(c) {
        cls = "Peep";
        x = randf() * W;
        y = randf() * H;
        width = 80 * DRAWSCALE;
        height = 120 * DRAWSCALE;
        hop = randf();
        speed = 1 + randf() * .5f;
        direction = randf() * PI * 2;
        flip = 1;
        isWalking = true;
        loop = true;
        startWalking();
        moveStep(); // Peep.js ends its constructor with self.update()
    }

    void setTimeout(std::function<void()> fn, int ticks) { animCalls.push_back(AnimCall{fn, ticks}); }
    void clearAnims() { animCalls.clear(); }

    virtual void onStartWalking() {}
    virtual void onUpdate() {}
    virtual string shapeType() const { return ""; }
    virtual void beShocked() {}
    virtual void beConfused(Peep*) {}
    virtual void beOffended(Peep*) {}
    virtual void beStunned() {}
    virtual bool offendedFlag() const { return false; }

    virtual void startWalking() {
        isWalking = true;
        speed = 1 + randf() * .5f;
        direction = randf() * PI * 2;
        onStartWalking();
    }
    void stopWalking(bool halt = false) {
        if (halt) { velX = 0; velY = 0; }
        isWalking = false;
        hop = 0;
    }

    virtual void walkAnim() {
        hop += speed / 40.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)PI * 2.f;
        rotation = std::sin(t) * 0.1f;
        pivotY = std::fabs(std::sin(t)) * 10.f;
        if (lastHop < 0.5f && hop >= 0.5f) bounce = 1.2f;
        if (lastHop > 0.9f && hop <= 0.1f) bounce = 1.2f;
    }
    virtual void standAnim() { rotation = 0; pivotY = 0; }
    virtual void watchTV() {}

    void moveStep() {
        float vx = 0, vy = 0;
        if (isWalking) { vx = std::cos(direction) * speed; vy = std::sin(direction) * speed; }
        velX = velX * 0.9f + vx * 0.1f;
        velY = velY * 0.9f + vy * 0.1f;
        x += velX;
        y += velY;

        if (loop) {
            const float margin = 50;
            if (x < -margin) x = W + margin;
            if (x > W + margin) x = -margin;
            if (y < 0) y = H + margin * 2;
            if (y > H + margin * 2) y = 0;
        }

        if (ctx && !goThroughSpots) {
            for (const Spot& spot : ctx->avoidSpots) {
                float dx = spot.x - x, dy = spot.y - y;
                if (dx * dx + dy * dy < spot.radius * spot.radius)
                    direction = std::atan2(dy, dx) + (float)PI;
            }
        }

        if (!(pauseAnimsWhenNotWatching && ctx && ctx->director && ctx->director->isWatchingTV)) {
            for (size_t i = 0; i < animCalls.size();) {
                if (--animCalls[i].ticks <= 0) {
                    auto fn = animCalls[i].fn;
                    animCalls.erase(animCalls.begin() + std::ptrdiff_t(i));
                    if (fn) fn();
                } else {
                    ++i;
                }
            }
        }

        if (isWalking) walkAnim(); else standAnim();

        bounceVel += (1 - bounce) * 0.2f;
        bounce += bounceVel;
        bounceVel *= 0.6f;
        lastHop = hop;
    }

    void update() override { moveStep(); onUpdate(); }

    vector<Peep*> touchingPeeps(float radius, std::function<bool(Peep*)> filter = {}, bool oneSided = false) {
        vector<Peep*> out;
        if (!ctx || !ctx->world) return out;
        for (Actor* a : ctx->world->peeps) {
            if (a == this || a->dead) continue;
            Peep* other = static_cast<Peep*>(a);
            float dx = other->x - x;
            float dy = (other->y - y) * 2.f;
            if (dx * dx + dy * dy < radius * radius) {
                if (oneSided) {
                    if (flip > 0 && other->x > x) out.push_back(other);
                    if (flip < 0 && other->x < x) out.push_back(other);
                } else {
                    out.push_back(other);
                }
            }
        }
        if (filter) out.erase(std::remove_if(out.begin(), out.end(),
            [&](Peep* p) { return !filter(p); }), out.end());
        return out;
    }

    void stayWithinRect(float l, float r, float t, float b, float turn) {
        if (x < l && velX < 0) direction += turn;
        if (x > r && velX > 0) direction += turn;
        if (y < t && velY < 0) direction += turn;
        if (y > b && velY > 0) direction += turn;
    }

    // Draw one sprite layer through this peep's graphics container transform.
    // neutralFlip reproduces a child sprite whose own scale.x is multiplied by flip (SnobbyPeep's word).
    void layer(const char* sheet, int frame, float ax = 0.5f, float ay = 1.f, float alpha = 1.f,
               bool neutralFlip = false) {
        SDL_Rect src = spriteFrame(sheet, frame);
        float cw = src.w * DRAWSCALE, ch = src.h * DRAWSCALE;
        float lx = (0.5f - ax) * cw, ly = (0.5f - ay) * ch;
        float sx = bounce * (neutralFlip ? 1.f : flip), sy = 1.f / bounce;
        float qx = lx * sx, qy = (ly - pivotY) * sy;
        float ca = std::cos(rotation), sa = std::sin(rotation);
        float px = x + qx * ca - qy * sa;
        float py = y + qx * sa + qy * ca;
        drawTex(T(sheet), &src, px, py, std::fabs(cw * sx), std::fabs(ch * sy),
                rotation * 180.f / (float)PI, sx < 0, alpha);
    }
};

// ---------------------------------------------------------------- NormalPeep
struct NormalPeep : Peep {
    string type = "???";
    int hatFrame = 0, bodyFrame = 0, faceFrame = 0, doubles = 0;
    bool stunned = false, shocked = false, confused = false, offended = false, wearingHat = false;

    explicit NormalPeep(SceneCtx* c) : Peep(c) { cls = "NormalPeep"; }

    void setType(const string& t) {
        type = t;
        bodyFrame = (t == "circle") ? 0 : 1;
    }
    string shapeType() const override { return type; }
    bool offendedFlag() const override { return offended; }

    void onUpdate() override {
        doubles = (doubles + 1) % 3;
        if (doubles == 0) {
            if (wearingHat && hatFrame < 15) {
                ++hatFrame;
                if (hatFrame == 11) {
                    bounce = 1.6f;
                    audioPlay("squeak", false, 1.f);
                }
                if (hatFrame == 15) faceFrame = 6;
            }
            if (!wearingHat && hatFrame >= 15) {
                ++hatFrame;
                if (hatFrame == 26) hatFrame = 0;
            }
        }
    }

    void startWalking() override {
        if (stunned) return;
        Peep::startWalking();
    }

    void beStunned() {
        stunned = true;
        stopWalking();
        faceFrame = 3;
    }

    void beShocked() {
        shocked = true;
        stopWalking();
        bounce = 2;
        faceFrame = 3;
        setTimeout([this] {
            if (dead) return;
            faceFrame = 0;
            startWalking();
            shocked = false;
        }, sec60(2.f));
    }

    void beConfused(Peep* target) {
        flip = (target->x > x) ? 1 : -1;
        confused = true;
        stopWalking();
        bounce = 1.1f;
        faceFrame = 2;
        setTimeout([this] { if (!dead) faceFrame = 4; }, sec60(0.2f));
        setTimeout([this] {
            if (dead) return;
            faceFrame = 2;
            setTimeout([this] { if (!dead) faceFrame = 0; }, sec60(0.2f));
            startWalking();
            confused = false;
        }, sec60(2.2f));
    }

    void beOffended(Peep* target) {
        flip = (target->x > x) ? 1 : -1;
        offended = true;
        stopWalking();
        clearAnims();
        bounce = 1.2f;
        faceFrame = 2;
        setTimeout([this] { if (!dead) faceFrame = 0; }, sec60(0.15f));
        setTimeout([this] {
            if (dead) return;
            bounce = 1.2f;
            faceFrame = 2;
            setTimeout([this] { if (!dead) faceFrame = 5; }, sec60(0.15f));
        }, sec60(1.2f));
        setTimeout([this] { if (!dead) startWalking(); }, sec60(3.f));
        setTimeout([this] {
            if (dead) return;
            faceFrame = 2;
            setTimeout([this] {
                if (dead) return;
                faceFrame = 0;
                offended = false;
            }, sec60(0.2f));
        }, sec60(8.f));
    }

    void wearHat() {
        clearAnims();
        if (!ctx || !ctx->tv) return;
        stopWalking(true);
        faceFrame = 1;
        flip = (ctx->tv->x > x) ? 1 : -1;
        float wait = ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME + randf() * 0.4f;
        isWatching = true;
        float hatTime = ZOOM_OUT_1_TIME + (std::fabs(x - ctx->tv->x) - 60.f) / 100.f;
        setTimeout([this] { if (!dead) wearingHat = true; }, sec60(hatTime));
        setTimeout([this] {
            if (dead) return;
            isWatching = false;
            bounce = 1.2f;
            startWalking();
        }, sec60(wait + 1.f));
    }

    void takeOffHat(bool instant) {
        clearAnims();
        if (!instant) {
            if (!ctx || !ctx->tv) return;
            stopWalking(true);
            faceFrame = 1;
            flip = (ctx->tv->x > x) ? 1 : -1;
            float wait = 4.f + randf() * 0.4f;
            isWatching = true;
            setTimeout([this] {
                if (dead) return;
                wearingHat = false;
                bounce = 1.1f;
                faceFrame = 2;
                setTimeout([this] { if (!dead) faceFrame = 7; }, sec60(0.2f));
            }, sec60(1.75f + randf() * 0.75f));
            setTimeout([this] {
                if (dead) return;
                isWatching = false;
                bounce = 1.2f;
                startWalking();
                faceFrame = 0;
            }, sec60(wait + 0.06f));
        } else {
            faceFrame = 0;
            hatFrame = 0;
            wearingHat = false;
        }
    }

    void getOuttaTV() {
        shocked = false;
        confused = false;
        const float l = W * .5f - 220, r = W * .5f + 220, t = H * .5f - 110, b = H * .5f + 190;
        while (x > l && x < r && y > t && y < b) {
            x = randf() * W;
            y = randf() * H;
        }
        stopWalking(true);
        setTimeout([this] { if (!dead) startWalking(); }, sec60(ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME));
    }

    void watchTV() {
        clearAnims();
        if (!ctx || !ctx->tv) return;
        stopWalking(true);
        faceFrame = 1;
        flip = (ctx->tv->x > x) ? 1 : -1;
        float wait = ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME + randf() * 0.4f;
        isWatching = true;
        setTimeout([this] {
            if (dead) return;
            isWatching = false;
            faceFrame = 2;
            bounce = 1.2f;
        }, sec60(wait));
        setTimeout([this] {
            if (dead) return;
            faceFrame = 0;
            startWalking();
        }, sec60(wait + 0.06f));
    }

    void draw() override {
        layer("hat", hatFrame);
        layer("body", bodyFrame);
        layer("face", faceFrame);
    }
};

// ---------------------------------------------------------------- LoverPeep (LoverPeep.js)
struct LoverPeep : NormalPeep {
    int shirtFrame = 0;
    Peep* follows = nullptr;
    bool isEmbarrassed = false;
    bool showLoveHat = true;

    explicit LoverPeep(SceneCtx* c) : NormalPeep(c) {
        cls = "LoverPeep";
        faceFrame = 8;
        x = 1000;
        y = 580;
        onStartWalking();
        stopWalking();
        setTimeout([this] { if (!dead) startWalking(); }, sec60(4.f));
    }

    void setType(const string& t) {
        NormalPeep::setType(t);
        shirtFrame = (t == "circle") ? 0 : 1;
    }

    void follow(Peep* f) {
        follows = f;
        x = f->x;
        y = f->y;
    }

    void onUpdate() override {
        if (follows) {
            Peep* f = follows;
            float tx = f->x - std::cos(f->direction) * 20.f;
            float ty = f->y - std::sin(f->direction) * 20.f;
            const float margin = 50;
            float dx = tx - x;
            while (dx > 300) { tx -= W + margin * 2; dx = tx - x; }
            while (dx < -300) { tx += W + margin * 2; dx = tx - x; }
            float dy = ty - y;
            while (dy > 300) { ty -= H + margin * 2; dy = ty - y; }
            while (dy < -300) { ty += H + margin * 2; dy = ty - y; }
            direction = std::atan2(dy, dx);
        }
        if (follows && isEmbarrassed && x > 1100) {
            follows->kill();
            kill();
            return;
        }
        if (follows && x < -500) {
            follows->kill();
            kill();
        }
    }

    void onStartWalking() override {
        direction = 3.3f;
        speed = 1.3f;
    }

    void walkAnim() override {
        if (follows) hop += speed / 120.f;
        NormalPeep::walkAnim();
    }

    void makeEmbarrassed() {
        clearAnims();
        if (!ctx || !ctx->tv) return;
        x = ctx->tv->x;
        y = ctx->tv->y - 5 - randf();
        if (type == "square") x += 80; else x += 120;
        stopWalking(true);
        faceFrame = 9;
        flip = (ctx->tv->x > x) ? 1 : -1;
        float wait = 3.7f;
        isWatching = true;
        setTimeout([this] {
            if (dead) return;
            isWatching = false;
            faceFrame = 10;
            bounce = 1.2f;
        }, sec60(wait));
        setTimeout([this] {
            if (dead) return;
            isEmbarrassed = true;
            faceFrame = 11;
            startWalking();
            loop = false;
            direction = 0;
            speed = 3;
        }, sec60(wait + 0.06f));
    }

    void draw() override {
        layer("hat", hatFrame);
        layer("body", bodyFrame);
        layer("face", faceFrame);
        if (showLoveHat) layer("lovehat", 0);
        layer("lover_shirt", shirtFrame);
    }
};

// ---------------------------------------------------------------- HatPeep (HatPeep.js)
struct HatPeep : Peep {
    explicit HatPeep(SceneCtx* c) : Peep(c) { cls = "HatPeep"; }
    void onUpdate() override { stayWithinRect(100, 860, 100, 480, 0.05f); }
    void walkAnim() override {
        hop += speed / 40.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)PI * 2.f;
        rotation = std::sin(t) * 0.2f;
        pivotY = std::fabs(std::sin(t)) * 7.f;
    }
    void draw() override { layer("hatguy", 0); }
};

inline bool isNormalFamily(Peep* p) {
    return !std::strcmp(p->cls, "NormalPeep") || !std::strcmp(p->cls, "LoverPeep") ||
           !std::strcmp(p->cls, "PanicPeep");
}

// ---------------------------------------------------------------- CrazyPeep (CrazyPeep.js)
struct CrazyPeep : Peep {
    int bodyFrame = 0, doubles = 0, loopScreaming = -1;
    int metaGrace = 0, wanderGrace = 0, gracePeriod = 0;
    float wander = 0;

    explicit CrazyPeep(SceneCtx* c) : Peep(c) {
        cls = "CrazyPeep";
        x = 1200; y = 330; direction = PI; loop = false;
        metaGrace = sec60(3.5f);
        wanderGrace = sec60(2.f);
        gracePeriod = sec60(4.5f);
        speed = 0;
    }

    void onStartWalking() override { speed = 2.f; }

    void changeWander() { wander = randf() * 0.1f - 0.05f; }

    void onUpdate() override {
        if (metaGrace > 0) {
            if (--metaGrace == 0) speed = 2.f; // callbacks.startWalking()
            return;
        }
        if (gracePeriod > 0) --gracePeriod;
        if (wanderGrace > 0) --wanderGrace;
        doubles = (doubles + 1) % 2;
        if (wanderGrace <= 0) {
            direction += wander;
            if (randf() < 0.05f) changeWander();
            stayWithinRect(100, 860, 100, 480, 0.15f);
        }
        if (gracePeriod <= 0) {
            auto close = touchingPeeps(50, [](Peep* p) { return p->isWalking; });
            if (!close.empty() && isWalking) {
                isWalking = false;
                bodyFrame = 10;
                loopScreaming = 2;
                audioPlay("scream", false, 1.f);
                direction += PI;
                gracePeriod = sec60(1.f);
                for (Peep* o : close) {
                    if (!o->isWalking) continue;
                    o->velX = flip * 10;
                    o->flip = -flip;
                    o->beShocked();
                }
            }
        }
    }

    void walkAnim() override {
        hop += speed / 50.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)PI;
        pivotY = std::fabs(std::sin(t)) * 20.f;
        if (doubles == 0) {
            ++bodyFrame;
            if (bodyFrame > 9) bodyFrame = 0;
        }
    }

    void standAnim() override {
        rotation = 0;
        pivotY = 0;
        if (doubles == 0) {
            int next = bodyFrame + 1;
            if (loopScreaming > 0) {
                if (next > 19) { next = 16; --loopScreaming; }
            } else {
                if (next > 20) { next = 0; isWalking = true; --loopScreaming; }
            }
            bodyFrame = next;
        }
    }

    bool isScreaming() const { return loopScreaming >= 0; }

    void draw() override { layer("hangry", bodyFrame, 0.33f, 1.f); }
};

// ---------------------------------------------------------------- NervousPeep (NervousPeep.js)
struct NervousPeep : Peep {
    enum { STARE = 0, BLINK = 1, SHOCKED = 2, RUNAWAY = 3, CALMDOWN = 4 };
    int MODE = -1, faceFrame = 0, doubles = 0;
    bool isShocked = false;

    explicit NervousPeep(SceneCtx* c) : Peep(c) { cls = "NervousPeep"; faceFrame = 0; }

    void HACK_JUMPSTART() { MODE = BLINK; }

    void onUpdate() override {
        doubles = (doubles + 1) % 3;
        stayWithinRect(150, 810, 150, 450, 0.05f);
        if (doubles == 0) {
            switch (MODE) {
                case STARE: if (faceFrame < 3) ++faceFrame; break;
                case BLINK:
                    if (faceFrame < 6) ++faceFrame;
                    if (faceFrame == 6) { if (randf() < 0.05f) ++faceFrame; }
                    else if (faceFrame < 10) ++faceFrame;
                    else if (faceFrame == 10) { if (randf() < 0.05f) ++faceFrame; }
                    else if (faceFrame < 16) ++faceFrame;
                    else if (faceFrame == 16) faceFrame = 6;
                    break;
                case SHOCKED: if (faceFrame < 22) ++faceFrame; break;
                case RUNAWAY: if (faceFrame < 25) ++faceFrame; break;
                case CALMDOWN:
                    if (faceFrame < 29) ++faceFrame;
                    else { faceFrame = 6; MODE = BLINK; isShocked = false; }
                    break;
                default: break;
            }
        }

        if (!isShocked) {
            auto close = touchingPeeps(90, [](Peep* p) {
                return p->isWalking && p->shapeType() == "square";
            });
            if (!close.empty() && isWalking) {
                MODE = SHOCKED;
                isWalking = false;
                faceFrame = 17;
                isShocked = true;
                audioPlay("peep_gasp", false, 1.f);
                flip = (close[0]->x > x) ? 1 : -1;
                bounce = 1.5f;
                velX = -flip * 5;
                for (Peep* o : close) {
                    if (!o->isWalking) continue;
                    o->beConfused(this);
                }
                setTimeout([this] {
                    if (dead) return;
                    audioPlay("peep_hngh", false, 0.6f);
                    MODE = RUNAWAY;
                    flip *= -1;
                    bounce = 1.5f;
                    startWalking();
                    speed = 3.5f;
                    direction = (flip > 0) ? 0.f : (float)PI;
                    setTimeout([this] {
                        if (dead) return;
                        MODE = CALMDOWN;
                        onStartWalking();
                    }, sec60(3.f));
                }, sec60(1.f));
            }
        }
    }

    void onStartWalking() override { speed = 0.8f; }

    void watchTV() override {
        clearAnims();
        if (!ctx || !ctx->tv) return;
        stopWalking(true);
        flip = (ctx->tv->x > x) ? 1 : -1;
        const float OFFSET = 0;
        float wait = ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME;
        setTimeout([this] {
            if (dead) return;
            audioPlay("squeak", false, 1.f);
            bounce = 1.6f;
            MODE = STARE;
        }, sec60(OFFSET + 2.f));
        setTimeout([this] {
            if (dead) return;
            bounce = 1.3f;
            MODE = BLINK;
        }, sec60(OFFSET + wait));
        setTimeout([this] { if (!dead) startWalking(); }, sec60(OFFSET + wait + 1.f));
    }

    bool isScared() const { return isShocked; }

    void draw() override {
        layer("body", 0);
        layer("face_nervous", faceFrame);
    }
};

// ---------------------------------------------------------------- SnobbyPeep (SnobbyPeep.js)
struct SnobbyPeep : Peep {
    enum { STARE = 0, BLINK = 1, SMUG = 2, HMPH = 3, POP = 4, AWAY = 5 };
    int MODE = -1, faceFrame = 0, wordFrame = 0, doubles = 0;
    bool isSmug = false;
    int gracePeriod = -1;

    explicit SnobbyPeep(SceneCtx* c) : Peep(c) {
        cls = "SnobbyPeep";
        faceFrame = 0; wordFrame = 0; gracePeriod = -1;
    }

    void HACK_JUMPSTART() { MODE = BLINK; }
    string shapeType() const override { return "square"; }

    void onUpdate() override {
        doubles = (doubles + 1) % 3;
        stayWithinRect(100, 860, 100, 480, 0.05f);
        if (doubles == 0) {
            switch (MODE) {
                case STARE: if (faceFrame < 3) ++faceFrame; break;
                case BLINK: if (faceFrame < 7) ++faceFrame; break;
                case SMUG: if (faceFrame < 14) ++faceFrame; break;
                case HMPH:
                    if (faceFrame < 20) {
                        ++faceFrame;
                        if (faceFrame >= 18) {
                            if (wordFrame == 0) wordFrame = 1;
                            else { ++wordFrame; if (wordFrame > 3) wordFrame = 1; }
                        }
                    }
                    break;
                case POP:
                    wordFrame = 0;
                    if (faceFrame < 25) ++faceFrame;
                    break;
                case AWAY:
                    if (faceFrame < 29) ++faceFrame;
                    else { faceFrame = 7; MODE = BLINK; isSmug = false; }
                    break;
                default: break;
            }
        }

        if (!isSmug) {
            if (gracePeriod <= 0) {
                auto close = touchingPeeps(90, [](Peep* p) {
                    return isNormalFamily(p) && !static_cast<NormalPeep*>(p)->offended &&
                           p->shapeType() == "circle";
                });
                if (!close.empty() && isWalking) {
                    flip = (close[0]->x > x) ? 1 : -1;
                    bounce = 1.5f;
                    MODE = SMUG;
                    isWalking = false;
                    audioPlay("peep_huh", false, 1.f);
                    for (Peep* o : close) {
                        if (isNormalFamily(o) && static_cast<NormalPeep*>(o)->offended) continue;
                        o->beOffended(this);
                    }
                    setTimeout([this] {
                        if (dead) return;
                        MODE = HMPH;
                        isSmug = true;
                        audioPlay("peep_hmph", false, 1.f);
                        setTimeout([this] {
                            if (dead) return;
                            MODE = POP;
                            setTimeout([this] {
                                if (dead) return;
                                flip *= -1;
                                startWalking();
                                direction = (flip > 0) ? 0.f : (float)PI;
                                direction += randf() * 0.2f - 0.1f;
                                MODE = AWAY;
                                gracePeriod = 60;
                            }, sec60(0.8f));
                        }, sec60(1.4f));
                    }, sec60(0.9f));
                }
            } else {
                --gracePeriod;
            }
        }
    }

    void watchTV() override {
        clearAnims();
        if (!ctx || !ctx->tv) return;
        stopWalking(true);
        flip = (ctx->tv->x > x) ? 1 : -1;
        const float OFFSET = 0;
        float wait = ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME;
        setTimeout([this] {
            if (dead) return;
            audioPlay("squeak", false, 1.f);
            bounce = 1.6f;
            MODE = STARE;
        }, sec60(OFFSET + 2.f));
        setTimeout([this] {
            if (dead) return;
            bounce = 1.3f;
            MODE = BLINK;
        }, sec60(OFFSET + wait));
        setTimeout([this] { if (!dead) startWalking(); }, sec60(OFFSET + wait + 1.f));
    }

    void draw() override {
        layer("body", 1);
        layer("face_snobby", faceFrame, 0.5f, 1.25f);
        layer("face_snobby_hmph", wordFrame, 0.5f, 1.25f, 1.f, true);
    }
};

// ---------------------------------------------------------------- AngryPeep (AngryPeep.js)
struct AngryPeep : Peep {
    enum { STARE = 0, BLINK = 1, SHOUT = 2 };
    string type = "???";
    int bodyFrame = 0, faceFrame = 0, doubles = 0, MODE = -1;
    bool isShouting = false;
    int gracePeriod = -1;
    float bodyRedAlpha = 0.f, wander = 0.f;

    AngryPeep(SceneCtx* c, const string& t) : Peep(c) {
        cls = "AngryPeep";
        setType(t);
        gracePeriod = -1;
        onStartWalking();
    }

    void setType(const string& t) {
        type = t;
        bodyFrame = (t == "circle") ? 0 : 1;
    }
    string shapeType() const override { return type; }

    void HACK_JUMPSTART() { MODE = BLINK; }
    void changeWander() { wander = randf() * 0.1f - 0.05f; }
    void onStartWalking() override { speed = 1.7f; }

    void onUpdate() override {
        doubles = (doubles + 1) % 3;
        direction += wander;
        if (randf() < 0.05f) changeWander();
        stayWithinRect(100, 860, 100, 480, 0.15f);

        if (doubles == 0) {
            switch (MODE) {
                case STARE: if (faceFrame < 5) ++faceFrame; break;
                case BLINK: if (faceFrame < 10) ++faceFrame; break;
                case SHOUT:
                    if (faceFrame < 25) ++faceFrame;
                    else {
                        faceFrame = 10;
                        MODE = BLINK;
                        isShouting = false;
                        startWalking();
                    }
                    break;
                default: break;
            }
        }

        if (faceFrame >= 10) bodyRedAlpha = bodyRedAlpha * 0.9f + 0.1f;

        if (gracePeriod <= 0 && !(ctx && ctx->noYellingYet)) {
            string opposite = (type == "circle") ? "square" : "circle";
            auto close = touchingPeeps(90, [&](Peep* p) {
                return !p->offendedFlag() && p->isWalking && p->shapeType() == opposite;
            });
            if (isWalking && !close.empty()) {
                Peep* other = close[0];
                flip = (other->x > x) ? 1 : -1;
                bounce = 1.7f;
                velY = 0;
                velX = 3 * flip;
                MODE = SHOUT;
                isWalking = false;
                isShouting = true;
                int angryCount = 0;
                if (ctx && ctx->world) {
                    for (Actor* a : ctx->world->peeps)
                        if (!std::strcmp(a->cls, "AngryPeep")) ++angryCount;
                }
                float shoutVol = angryCount < 5 ? 1.f : angryCount < 10 ? 0.7f : angryCount < 15 ? 0.42f : 0.27f;
                audioPlay("shout", false, shoutVol);
                gracePeriod = sec60(2.5f);
                if (!std::strcmp(other->cls, "NormalPeep")) {
                    NormalPeep* np = static_cast<NormalPeep*>(other);
                    if (np->shocked) return;
                    np->velX = flip * 5;
                    np->flip = -flip;
                    np->beShocked();
                }
            }
        } else {
            --gracePeriod;
        }
    }

    void walkAnim() override {
        hop += speed / 40.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)PI * 2.f;
        pivotY = std::fabs(std::sin(t)) * 15.f;
        rotation = 0;
        if (lastHop < 0.5f && hop >= 0.5f) bounce = 1.2f;
        if (lastHop > 0.9f && hop <= 0.1f) bounce = 1.2f;
    }

    void watchTV() override {
        clearAnims();
        if (!ctx || !ctx->tv) return;
        stopWalking(true);
        flip = (ctx->tv->x > x) ? 1 : -1;
        float OFFSET = (std::fabs(x - ctx->tv->x) - 60.f) / 100.f;
        float wait = ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME;
        setTimeout([this] {
            if (dead) return;
            audioPlay("squeak", false, 1.f);
            bounce = 1.6f;
            MODE = STARE;
        }, sec60(2.f + OFFSET));
        setTimeout([this] {
            if (dead) return;
            bounce = 1.3f;
            MODE = BLINK;
        }, sec60(wait + OFFSET));
        setTimeout([this] { if (!dead) startWalking(); }, sec60(wait + 1.f + OFFSET));
    }

    void draw() override {
        layer("body", bodyFrame);
        layer("body_red", bodyFrame, 0.5f, 1.f, bodyRedAlpha);
        layer("face_angry", faceFrame, 0.3333333f, 1.f);
    }
};

// ---------------------------------------------------------------- HappyWeirdoPeep
struct HappyWeirdoPeep : Peep {
    enum { SMILE = 1, FROWN = 2 };
    int MODE = -1, bodyFrame = 0, doubles = 0;
    float wander = 0;

    explicit HappyWeirdoPeep(SceneCtx* c) : Peep(c) {
        cls = "HappyWeirdoPeep";
        x = 115; y = 170;
        onStartWalking();
        hop = 0; lastHop = 0.99f; direction = 1;
    }

    void smile() { MODE = SMILE; }
    void frown() { MODE = FROWN; }
    void changeWander() { wander = randf() * 0.1f - 0.05f; }
    void onStartWalking() override { speed = 2.f; }

    void onUpdate() override {
        direction += wander;
        if (randf() < 0.05f) changeWander();
        stayWithinRect(100, 860, 100, 480, 0.15f);
        doubles = (doubles + 1) % 4;
        if (doubles == 0) {
            if (MODE == SMILE && bodyFrame < 2) ++bodyFrame;
            if (MODE == FROWN && bodyFrame < 5) ++bodyFrame;
        }
    }

    void walkAnim() override {
        hop += speed / 50.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)PI;
        pivotY = std::fabs(std::sin(t)) * 20.f;
        rotation = 0;
        if (lastHop > 0.9f && hop <= 0.1f) bounce = 1.2f;
    }

    void standAnim() override {
        hop += speed / 150.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        if (MODE != FROWN) {
            float t = hop * (float)TAU;
            rotation = std::sin(t) * 0.1f;
            pivotY = 0;
        }
    }

    void prepareForMurder() {
        stopWalking();
        x = 540; y = 470;
        velX = -1; velY = 0;
        flip = -1;
        if (ctx) ctx->avoidSpots.push_back(Spot{480, 430, 150});
    }

    void draw() override { layer("happy_weirdo", bodyFrame, 0.35f, 1.f); }
};

// ---------------------------------------------------------------- EvilHatPeep (EvilHatPeep.js)
struct EvilHatPeep : Peep {
    int gunFrame = 0, doubles = 0, MODE = 0; // 0 walk, 1 gun
    bool isMurdering = false, hasGunOut = false;
    Peep* victim = nullptr;
    std::function<void()> freezeEveryone, bang;

    explicit EvilHatPeep(SceneCtx* c) : Peep(c) {
        cls = "EvilHatPeep";
        loop = false;
        x = -300; y = 470;
        direction = 0;
        goThroughSpots = true;
        pauseAnimsWhenNotWatching = true;
    }

    void itsMurderTime() {
        isMurdering = true;
        stopWalking();
        bounce = 1.2f;
        Peep* v = victim;
        setTimeout([v] { if (v && !v->dead) static_cast<HappyWeirdoPeep*>(v)->smile(); }, sec60(0.5f));
        setTimeout([this] {
            if (dead) return;
            hasGunOut = true;
            MODE = 1;
        }, sec60(2.f));
        setTimeout([this, v] {
            if (dead) return;
            if (freezeEveryone) freezeEveryone();
            if (v && !v->dead) static_cast<HappyWeirdoPeep*>(v)->frown();
        }, sec60(2.5f));
        setTimeout([this] {
            if (dead) return;
            if (bang) bang();
        }, sec60(6.f));
    }

    void onUpdate() override {
        if (x > 420 && !isMurdering) itsMurderTime();
        doubles = (doubles + 1) % 3;
        if (doubles == 0 && MODE == 1) {
            if (gunFrame < 17) ++gunFrame;
        }
        if (MODE == 0) speed = (ctx && ctx->director && ctx->director->isWatchingTV) ? 0.f : 1.25f;
    }

    void walkAnim() override {
        hop += speed / 40.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)TAU;
        rotation = std::sin(t) * 0.2f;
        pivotY = std::fabs(std::sin(t)) * 7.f;
    }

    void draw() override {
        layer("gun", gunFrame);
        layer("hatguy", 0);
    }
};

// ---------------------------------------------------------------- particles (Blood/Gore/DeadBody)
void shakeScreen(SceneCtx* ctx, float intensity); // defined with the game scene

struct Blood : Actor {
    int frame = 0;
    float sc = 0.f, gotoScale = 0.f;
    bool done = false;
    explicit Blood(SceneCtx*) {
        cls = "Blood";
        frame = int(randf() * 3);
    }
    void init(float px, float py, float scale) { x = px; y = py; gotoScale = scale; }
    void update() override {
        if (done) return;
        if (std::fabs(sc - gotoScale) < 0.01f) done = true;
        sc = sc * 0.8f + gotoScale * 0.2f;
    }
    void draw() override {
        SDL_Rect src = spriteFrame("blood", frame);
        drawTex(T("blood"), &src, x, y, src.w * sc, src.h * sc);
    }
};

struct Gore : Actor {
    int frame = 0;
    float rotation = 0, direction = 0, velocity = 0, vx = 0, vy = 0, vz = 0;
    float gravity = 0.5f, vr = 0;
    explicit Gore(SceneCtx*) {
        cls = "Gore";
        frame = int(randf() * 3);
    }
    void init(float dir, float vel, float px, float py, float pz, float g = 0.5f) {
        direction = dir; velocity = vel; x = px; y = py; z = pz; gravity = g;
        vx = std::cos(dir) * vel;
        vz = std::sin(dir) * vel;
        vy = randf() - 0.5f;
        vr = (randf() * 2 - 1) * 0.5f;
    }
    void update() override {
        x += vx; y += vy; z += vz; rotation += vr; vz += gravity;
        if (z >= 0) {
            z = 0; vx *= 0.8f; vy *= 0.8f; vr *= 0.8f;
            if (std::fabs(vz) > 1) {
                if (world) {
                    Blood* b = world->make<Blood>(world->ctx);
                    b->init(x, y, std::fabs(vz) * 0.02f);
                    world->addBG(b);
                }
                vz *= -0.2f;
            } else {
                vz = 0;
            }
        }
    }
    void draw() override {
        SDL_Rect src = spriteFrame("gore", frame);
        drawTex(T("gore"), &src, x, y + z, src.w * DRAWSCALE, src.h * DRAWSCALE,
                rotation * 180.f / (float)PI);
    }
};

struct DeadBody : Actor {
    int frame = 0;
    float rotation = 0, direction = 0, velocity = 0, vx = 0, vz = 0, gravity = 0.2f, vr = 0, flip = 1;
    explicit DeadBody(SceneCtx*) {
        cls = "DeadBody";
        z = -10;
        gravity = 0.2f;
    }
    void init(float dir, float vel, float px, float py, float pz, float flipv, int frameIdx) {
        direction = dir; velocity = vel; x = px; y = py; z = pz; flip = flipv; frame = frameIdx;
        vx = std::cos(dir) * vel;
        vz = std::sin(dir) * vel;
        if (world) {
            Blood* b = world->make<Blood>(world->ctx);
            b->init(x, y, 1.f);
            world->addBG(b);
        }
    }
    void update() override {
        x += vx; z += vz; vz += gravity;
        if (z >= 0) {
            z = 0; vx *= 0.8f; vr *= 0.8f;
            if (std::fabs(vz) > 1) {
                if (world) {
                    Blood* b = world->make<Blood>(world->ctx);
                    b->init(x, y, 0.5f + std::fabs(vz) * 0.1f);
                    world->addBG(b);
                }
                vz *= -0.3f;
            } else {
                vz = 0;
            }
        }
        rotation = (std::fabs(z) / 50.f) * ((float)TAU / 4.f);
        if (rotation > (float)TAU * 0.2f) rotation = (float)TAU * 0.2f;
    }
    void draw() override {
        SDL_Rect src = spriteFrame("gore_bodies", frame);
        float ax = 132.f / 160.f, ay = 91.f / 120.f;
        float w = src.w * DRAWSCALE, h = src.h * DRAWSCALE;
        float cx = x + (0.5f - ax) * w * flip;
        float cy = (y + z) + (0.5f - ay) * h;
        drawTex(T("gore_bodies"), &src, cx, cy, w, h, rotation * 180.f / (float)PI, flip < 0);
    }
};

// ---------------------------------------------------------------- PanicPeep (PanicPeep.js)
struct PanicPeep : NormalPeep {
    bool isLover = false;
    int loverFrame = 0;

    explicit PanicPeep(SceneCtx* c) : NormalPeep(c) {
        cls = "PanicPeep";
        faceFrame = 12;
        onStartWalking();
        startWalking();
    }

    void onStartWalking() override { speed = 3.f + randf() * 2.f; }

    void setLover(const string& type) {
        if (type == "circle") { x = 635; y = 200; direction = (float)TAU * -0.12f; }
        else { x = 665; y = 200; direction = (float)TAU * -0.10f; }
        loverFrame = (type == "circle") ? 0 : 1;
        loop = false;
        isLover = true;
    }

    void onUpdate() override {
        if (isLover && y < -500) kill();
    }

    void walkAnim() override {
        hop += speed / 60.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)PI * 2.f;
        rotation = std::sin(t) * 0.3f;
        pivotY = std::fabs(std::sin(t)) * 15.f;
        if (lastHop < 0.5f && hop >= 0.5f) bounce = 1.2f;
        if (lastHop > 0.9f && hop <= 0.1f) bounce = 1.2f;
    }

    void getKilledBy(Peep* killer, const string& weapon);

    void draw() override {
        layer("hat", hatFrame);
        layer("body", bodyFrame);
        layer("face", faceFrame);
        if (isLover) layer("lover_panic", loverFrame);
    }
};

// ---------------------------------------------------------------- MurderPeep (MurderPeep.js)
struct MurderPeep : Peep {
    enum { STARE = 0, CRAZY = 1, KILL = 2 };
    string type = "???", weaponType;
    int bodyFrame = 0, faceFrame = 0, weaponFrame = 0, MODE = STARE, doubles = 0;
    int gracePeriod = -1, standingTime = -1;

    explicit MurderPeep(SceneCtx* c) : Peep(c) {
        cls = "MurderPeep";
        gracePeriod = -1;
        standingTime = -1;
    }

    string shapeType() const override { return type; }
    void onStartWalking() override { speed = 3.f; }

    void init(const string& shapeTypeIn, const string& weapon) {
        type = shapeTypeIn;
        bodyFrame = (type == "circle") ? 0 : 1;
        weaponType = weapon;
        weaponFrame = 0;
        if (!ctx || !ctx->tv) return;
        x = ctx->tv->x;
        y = ctx->tv->y + randf();
        if (type == "circle") { x -= 60; flip = 1; gracePeriod = 30; }
        else { x += 60; flip = -1; gracePeriod = 50; }
        watchTV();
    }

    void onUpdate() override {
        doubles = (doubles + 1) % 4;
        stayWithinRect(100, 860, 100, 480, 0.15f);

        if (doubles == 0) {
            if (MODE == STARE) { if (faceFrame < 4) ++faceFrame; else faceFrame = 0; }
            else if (MODE == CRAZY) { if (faceFrame < 13) ++faceFrame; else faceFrame = 9; }

            if (MODE == CRAZY || MODE == KILL) {
                if (weaponFrame < 5) ++weaponFrame;
                if (weaponFrame >= 6) {
                    if (weaponFrame < 15) ++weaponFrame;
                    else if (standingTime <= 0) { startWalking(); weaponFrame = 5; }
                    else --standingTime;
                }
            }
        }

        if (gracePeriod <= 0) {
            string otherType = (type == "circle") ? "square" : "circle";
            auto close = touchingPeeps(120, [&](Peep* p) {
                return !std::strcmp(p->cls, "PanicPeep") && p->shapeType() == otherType;
            });
            if (!close.empty() && isWalking) {
                flip = (close[0]->x > x) ? 1 : -1;
                MODE = KILL;
                weaponFrame = 6;
                gracePeriod = sec60(2.f);
                bounce = 1.6f;
                standingTime = 5;
                stopWalking();
                if (weaponType == "gun") audioPlay("gunshot", false, 0.2f);
                else if (weaponType == "shotgun") audioPlay("shotgun", false, 0.2f);
                else audioPlay("impact", false, 0.2f);
                static_cast<PanicPeep*>(close[0])->getKilledBy(this, weaponType);
            }
        } else if (isWalking) {
            --gracePeriod;
        }
    }

    void walkAnim() override {
        hop += speed / 40.f;
        if (hop > 1.f) hop -= 1.f;
        flip = (velX < 0) ? -1.f : 1.f;
        float t = hop * (float)PI * 2.f;
        pivotY = std::fabs(std::sin(t)) * 15.f;
        rotation = (randf() * 2 - 1) * 0.05f;
    }

    void standAnim() override {
        rotation = (randf() * 2 - 1) * 0.05f;
        pivotY = 0;
    }

    void watchTV() override {
        clearAnims();
        if (!ctx || !ctx->tv) return;
        stopWalking(true);
        flip = (ctx->tv->x > x) ? 1 : -1;
        float wait = ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME;
        setTimeout([this] {
            if (dead) return;
            bounce = 1.6f;
            MODE = CRAZY;
        }, sec60(2.f));
        setTimeout([this] {
            if (dead) return;
            startWalking();
            direction = (type == "circle") ? (float)PI + (randf() * 2 - 1) : (randf() * 2 - 1);
        }, sec60(wait + 1.f));
    }

    void draw() override {
        layer("body", bodyFrame);
        layer("face_murder", faceFrame);
        string sheet = "weapon_" + weaponType;
        if (!weaponType.empty()) layer(sheet.c_str(), weaponFrame);
    }
};

void PanicPeep::getKilledBy(Peep* killer, const string& weapon) {
    int corpseFrame = 0, goreAmount = 5;
    float corpseVelocity = 2;
    if (weapon == "gun") { corpseFrame = 0; corpseVelocity = 2; goreAmount = 5; }
    else if (weapon == "bat") { corpseFrame = 1; corpseVelocity = 5; goreAmount = 15; }
    else if (weapon == "shotgun") { corpseFrame = 2; corpseVelocity = 10; goreAmount = 30; }
    else if (weapon == "axe") { corpseFrame = 3; corpseVelocity = 5; goreAmount = 15; }

    shakeScreen(ctx, 30);

    float fl = (killer->x < x) ? -1.f : 1.f;
    int frameOffset = (type == "circle") ? 0 : 1;
    if (world) {
        DeadBody* db = world->make<DeadBody>(ctx);
        db->init(-(float)TAU / 4.f - fl * 0.7f, corpseVelocity, x, y, -10, fl, (corpseFrame + 2) * 2 + frameOffset);
        world->addProp(db);

        for (int i = 0; i < goreAmount; ++i) {
            Gore* g = world->make<Gore>(ctx);
            g->init(-(float)TAU / 4.f - fl * randf() * 0.5f, corpseVelocity + randf() * 7.f,
                    x, y, randf() * -30.f);
            world->addProp(g);
        }
    }

    string selfType = type;
    kill();

    if (!world) return;
    PanicPeep* np = world->make<PanicPeep>(ctx);
    np->setType(selfType);
    np->x = (randf() < 0.5f) ? -50.f : W + 50.f;
    np->y = H * randf();
    world->addPeep(np);
}

struct Cricket;
Cricket* spawnCricket(SceneCtx* ctx, float xOffset = 0, int timeoutBonus = 0);

// ---------------------------------------------------------------- Director implementation
void Director::takePhoto(Camera* camera) {
    photoTexture = camera->photoTexture;
    photoSrc = camera->photoSrc;
    tv = ctx->tv;

    mainTween = Tween();
    twWait(mainTween, sec60(0.25f));
    twCall(mainTween, [this] {
        camXTween = Tween();
        twTo(camXTween, sec60(1.f), [this] { return ctx->camera->x; }, W * .5f,
             [this](float v) { ctx->camera->x = v; }, Ease::CircInOut);
        camXTween.play();
        camYTween = Tween();
        twTo(camYTween, sec60(1.f), [this] { return ctx->camera->y; }, H * .5f,
             [this](float v) { ctx->camera->y = v; }, Ease::CircInOut);
        camYTween.play();
        camScaleTween = Tween();
        twTo(camScaleTween, sec60(1.f), [this] { return ctx->camera->scale; }, W / ctx->camera->width,
             [this](float v) { ctx->camera->scale = v; }, Ease::CircInOut);
        camScaleTween.play();
        auto moveFn = cbMovePhoto;
        if (moveFn) moveFn(*this);
    });
    twWait(mainTween, sec60(1.f));
    twCall(mainTween, [this] {
        isWatchingTV = true;
        ctx->camera->hide();
        bool fail = false;
        bool nothing = data.ITS_NOTHING;
        if (!data.forceChyron) {
            if (data.audience == 0 && data.audienceCircles == 0 && data.audienceSquares == 0) {
                fail = true;
                if (ctx->world) {
                    audioPlay("crickets", false, 1.f);
                    spawnCricket(ctx);
                    if (data.CAUGHT_A_CRICKET) {
                        spawnCricket(ctx, 30, 15);
                        spawnCricket(ctx, 60, 30);
                    }
                }
            }
        }
        if (tv) tv->placePhoto(photoTexture, chyron, fail, nothing, photoSrc);
        audienceMovePhoto();
        cutViewportTo(tv->x, tv->y - 113.5f, 8.f);
        vector<Actor*> copy = ctx->world->peeps;
        for (Actor* a : copy) {
            if (!std::strcmp(a->cls, "NormalPeep")) static_cast<NormalPeep*>(a)->getOuttaTV();
        }
    });
    twWait(mainTween, sec60(1.5f));
    twCall(mainTween, [this] {
        float x = (tv->x * 3 + W * .5f) / 4.f;
        float y = tv->y + (-113.5f) * 3.f / 4.f;
        if (ctx->world) tweenViewportTo(x, y, 2.5f, sec60(2.f), Ease::CubicInOut);
        auto cutFn = cbCutToTV;
        ckpt("d.cut");
        if (cutFn) cutFn(*this);
    });
    twWait(mainTween, sec60(2.f));
    twWait(mainTween, sec60(1.5f));
    twCall(mainTween, [this] {
        isWatchingTV = false;
        if (ctx->world) tweenViewportTo(W * .5f, H * .5f, 1.f, sec60(2.f), Ease::CubicInOut);
    });
    twWait(mainTween, sec60(2.f));
    twCall(mainTween, [this] {
        ctx->camera->reset();
        cutViewportTo(W * .5f, H * .5f, 1.f);
        auto resetFn = cbReset;
        if (resetFn) resetFn(*this);
    });
    mainTween.play();

    chyron = "[NO CHYRON]";
    data = PhotoData();
    auto takeFn = cbTakePhoto;
    if (takeFn) takeFn(*this);
}

void Director::update() {
    mainTween.update();
    camXTween.update();
    camYTween.update();
    camScaleTween.update();
    viewTween.update();
}

void Director::cutViewportTo(float vx, float vy, float s) {
    if (!ctx || !ctx->world) return;
    ctx->world->pivotX = vx;
    ctx->world->pivotY = vy;
    ctx->world->viewScale = s;
}

void Director::tweenViewportTo(float vx, float vy, float s, int ticks, Ease e) {
    if (!ctx || !ctx->world) return;
    World* w = ctx->world;
    vsl = w->pivotX - ((W * .5f) / w->viewScale);
    vsr = w->pivotX + ((W * .5f) / w->viewScale);
    vst = w->pivotY - ((H * .5f) / w->viewScale);
    vsb = w->pivotY + ((H * .5f) / w->viewScale);
    vel = vx - ((W * .5f) / s);
    ver = vx + ((W * .5f) / s);
    vet = vy - ((H * .5f) / s);
    veb = vy + ((H * .5f) / s);

    viewT = 0.f;
    viewTween = Tween();
    twTo(viewTween, ticks, [this] { return viewT; }, 1.f, [this](float v) {
        viewT = v;
        float t = viewT * viewT; // Director.js squares the eased value
        float pl = vsl + (vel - vsl) * t;
        float pr = vsr + (ver - vsr) * t;
        float pt = vst + (vet - vst) * t;
        float pb = vsb + (veb - vsb) * t;
        ctx->world->pivotX = (pl + pr) * .5f;
        ctx->world->pivotY = (pt + pb) * .5f;
        ctx->world->viewScale = W / (pr - pl);
    }, e);
    viewTween.play();
}

vector<Actor*> Director::inCamera(std::function<bool(Actor*)> filter) {
    vector<Actor*> out;
    if (!ctx || !ctx->world || !ctx->camera) return out;
    Camera* cam = ctx->camera;
    float cl = cam->x - cam->width * .5f, cr = cam->x + cam->width * .5f;
    float ct = cam->y - cam->height * .5f, cb = cam->y + cam->height * .5f;
    for (Actor* prop : ctx->world->props) {
        if (prop->dead || prop->width <= 0 || prop->height <= 0) continue;
        float realY = prop->y + prop->z;
        float pl = prop->x - prop->width * .5f, pr = prop->x + prop->width * .5f;
        float pt = realY - prop->height, pb = realY;
        if (pr < cl || pl > cr || pb < ct || pt > cb) continue;
        float l = std::max(cl, pl), t = std::max(ct, pt);
        float r = std::min(cr, pr), b = std::min(cb, pb);
        float overlapRatio = ((r - l) * (b - t)) / (prop->width * prop->height);
        if (overlapRatio > 0.33f) out.push_back(prop);
    }
    if (filter) out.erase(std::remove_if(out.begin(), out.end(),
        [&](Actor* a) { return !filter(a); }), out.end());
    return out;
}

bool Director::audienceCutToTV(std::function<void(Peep*)> doElse, std::function<bool(Peep*)> filterAudience) {
    PhotoData& d = data;
    int circlesLeft = d.HIJACK ? d.audienceCircles : d.audience;
    int squaresLeft = d.HIJACK ? d.audienceSquares : d.audience;
    if (d.HIJACK) {
        d.audience = d.audienceCircles + d.audienceSquares;
    } else {
        d.audienceCircles = d.audience;
        d.audienceSquares = d.audience;
    }
    if (!ctx || !ctx->world) return false;
    vector<Peep*> peeps;
    for (Actor* a : ctx->world->peeps) peeps.push_back(static_cast<Peep*>(a));

    for (Peep* p : peeps) {
        if (!std::strcmp(p->cls, "NormalPeep")) static_cast<NormalPeep*>(p)->getOuttaTV();
    }

    if (d.audience > 0) {
        if (filterAudience) peeps.erase(std::remove_if(peeps.begin(), peeps.end(),
            [&](Peep* p) { return !filterAudience(p); }), peeps.end());
        for (size_t i = peeps.size(); i > 1; --i) std::swap(peeps[i - 1], peeps[size_t(randf() * float(i))]);

        for (Peep* p : peeps) {
            if (std::strcmp(p->cls, "NormalPeep")) continue;
            NormalPeep* np = static_cast<NormalPeep*>(p);
            int flipDir = 0;
            bool watchTV = false;
            float offset = 0;
            if (np->type == "circle" && circlesLeft > 0) {
                flipDir = 1;
                offset = 60.f + (d.audienceCircles - circlesLeft) * 40.f;
                watchTV = true;
                --circlesLeft;
            }
            if (np->type == "square" && squaresLeft > 0) {
                flipDir = -1;
                offset = 60.f + (d.audienceSquares - squaresLeft) * 40.f;
                watchTV = true;
                --squaresLeft;
            }
            if (watchTV) {
                np->x = ctx->tv->x - flipDir * offset;
                np->y = ctx->tv->y + randf();
                if (doElse) doElse(np); else np->watchTV();
            }
        }
    }
    return d.audience > 0;
}

void World::addBalancedPeeps(int howMany) {
    int circleCount = 0, squareCount = 0;
    for (Actor* a : peeps) {
        if (std::strcmp(a->cls, "NormalPeep")) continue;
        NormalPeep* p = static_cast<NormalPeep*>(a);
        if (p->type == "circle") ++circleCount;
        if (p->type == "square") ++squareCount;
    }
    for (int i = 0; i < howMany; ++i) {
        NormalPeep* peep = make<NormalPeep>(ctx);
        if (circleCount < squareCount) { peep->setType("circle"); ++circleCount; }
        else { peep->setType("square"); ++squareCount; }
        addPeep(peep);
    }
}

// ---------------------------------------------------------------- ScreenShake / ScreenZoomOut
struct ScreenShake {
    bool hasSnow = false;
    float intensity = 0, baseAlpha = 0.15f, snowAlpha = 1;
    float snowScaleX = 1, snowScaleY = 1;
    float offX = 0, offY = 0;

    void shake(float i) {
        intensity = i;
        if (!hasSnow) { hasSnow = true; snowAlpha = 1; }
        else snowAlpha = baseAlpha + 0.35f;
    }

    void update(float sceneScale) {
        if (hasSnow) {
            snowAlpha = snowAlpha * 0.95f + baseAlpha * 0.05f;
            snowScaleX = 1 + randf() * 0.2f;
            snowScaleY = 1 + randf() * 0.2f;
            if (randf() < 0.5f) snowScaleX *= -1;
            if (randf() < 0.5f) snowScaleY *= -1;
        }
        if (intensity <= 0.5f) {
            offX = 0;
            offY = 0;
            intensity = 0;
        } else {
            offX = sceneScale * (randf() * 2 - 1) * intensity;
            offY = sceneScale * (randf() * 2 - 1) * intensity;
            intensity *= 0.95f;
        }
    }

    void draw() {
        if (!hasSnow) return;
        drawTex(T("snow"), nullptr, 480.f - offX, 270.f - offY,
                960.f * std::fabs(snowScaleX), 540.f * std::fabs(snowScaleY),
                0, snowScaleX * snowScaleY < 0, snowAlpha);
    }
};

struct ScreenZoomOut {
    bool started = false, completed = false;
    float scale = 1;
    int fullTimer = sec60(40.f), timer = sec60(40.f);
    std::function<void()> onComplete;

    void init() { if (!started) started = true; }

    void update(float& sceneScale, float& sx, float& sy) {
        if (completed || !started) return;
        scale *= 0.9996f;
        sceneScale = scale;
        sx = (W - W * scale) * .5f;
        sy = (H - H * scale) * .5f;
        if (--timer <= 0) {
            completed = true;
            if (onComplete) onComplete();
        }
    }
};

struct Cricket : Actor {
    int frame = 0, period = 10, breathe = 0, mode = 0; // mode 0 chirp, 1 hop
    float flip = 1, hop = 0;
    int hopAwayTimeout = -1;

    explicit Cricket(SceneCtx*) {
        cls = "Cricket";
        width = 137 * 0.25f;
        height = 137 * 0.25f;
        flip = 1;
        breathe = int(randf() * period);
    }

    void watchTV() {
        if (!world || !world->ctx || !world->ctx->tv) return;
        x = world->ctx->tv->x + 100;
        y = world->ctx->tv->y;
        flip = -1;
        float wait = ZOOM_OUT_1_TIME + SEE_VIEWERS_TIME + 2.3f;
        hopAwayTimeout = sec60(wait);
    }

    void update() override {
        float sc = 1.f;
        if (mode == 0) {
            ++breathe;
            if (breathe > period + 10) breathe = 0;
            if (breathe > period) {
                switch (breathe % 4) {
                    case 0: sc = 1.1f; break;
                    case 1: sc = 1.0f; break;
                    case 2: sc = 0.9f; break;
                    default: sc = 1.0f; break;
                }
            }
        }
        if (hopAwayTimeout > 0) {
            if (--hopAwayTimeout == 0) mode = 1;
        }
        if (mode == 1) {
            if (world && world->ctx && world->ctx->tv) {
                flip = 1;
                x += 3.5f;
                hop += 0.1570795f;
                z = -std::fabs(std::sin(hop)) * 100.f;
                y = world->ctx->tv->y;
            }
        }
        if (x > W + 50) kill();
        breatheScale = sc;
    }
    float breatheScale = 1.f;

    void draw() override {
        SDL_Rect src = spriteFrame("cricket", frame);
        float sc = 0.25f * (mode == 0 ? breatheScale : 1.f);
        float w = src.w * sc, h = src.h * sc;
        drawTex(T("cricket"), &src, x, y + z - h * .5f, w, h, 0.f, flip < 0);
    }
};

void shakeScreen(SceneCtx* ctx, float intensity) {
    if (ctx && ctx->shaker) ctx->shaker->shake(intensity);
}

Cricket* spawnCricket(SceneCtx* ctx, float xOffset, int timeoutBonus) {
    if (!ctx || !ctx->world) return nullptr;
    Cricket* c = ctx->world->make<Cricket>(ctx);
    ctx->world->addProp(c); // sets c->world, which watchTV() needs
    c->watchTV();
    c->x += xOffset;
    c->hopAwayTimeout += timeoutBonus;
    return c;
}

// ---------------------------------------------------------------- AnimationProp + Helping/Protest
struct AnimationProp : Actor {
    SceneCtx* ctx = nullptr;
    string sheet;
    int frame = 0;

    explicit AnimationProp(SceneCtx* c) : ctx(c) { cls = "AnimationProp"; }

    void init(float px, float py, const char* name) {
        sheet = name;
        x = px; y = py;
        SDL_Rect f = spriteFrame(name, 0);
        width = f.w * DRAWSCALE;
        height = f.h * DRAWSCALE;
    }

    void update() override { updateAnimation(); }
    virtual void updateAnimation() {}

    void draw() override {
        SDL_Rect f = spriteFrame(sheet.c_str(), frame);
        float w = f.w * DRAWSCALE, h = f.h * DRAWSCALE;
        drawTex(T(sheet.c_str()), &f, x, y - h * .5f, w, h);
    }
};

struct HelpingAnim : AnimationProp {
    bool hasHelped = false, loversGone = false;
    int gracePeriod = 0, frame2 = 0, triples = 0;
    Spot spot;

    explicit HelpingAnim(SceneCtx* c) : AnimationProp(c) {
        cls = "HelpingAnim";
        init(180, 170, "helping");
        spot = Spot{180, 130, 170};
        c->avoidSpots.push_back(spot);
        c->noYellingYet = true;
        gracePeriod = sec60(ZOOM_OUT_2_TIME * 0.8f);
    }

    void updateAnimation() override {
        if (ctx->director && ctx->director->isWatchingTV) gracePeriod = sec60(ZOOM_OUT_2_TIME * 0.9f);
        if (gracePeriod > 0) { --gracePeriod; return; }
        triples = (triples + 1) % 3;
        if (triples == 0 && !(ctx->director && ctx->director->isWatchingTV)) {
            if (frame < 110) {
                ++frame;
                if (frame == 12) audioPlay("scream", false, 1.f);
                if (frame == 70) {
                    hasHelped = true;
                    audioPlay("squeak", false, 1.f);
                }
            } else {
                ++frame2;
                if (frame2 > 20) { frame = 111; byeLovers(); }
                if (frame2 > 170) byeSelf();
            }
        }
    }

    void byeLovers() {
        if (loversGone) return;
        loversGone = true;
        if (!world) return;

        LoverPeep* l1 = world->make<LoverPeep>(ctx);
        l1->x = 188; l1->y = 165;
        l1->setType("circle");
        l1->startWalking();
        l1->showLoveHat = false;
        l1->faceFrame = 6;
        l1->bounceVel = 0.22f;
        l1->loop = false;

        LoverPeep* l2 = world->make<LoverPeep>(ctx);
        l2->follow(l1);
        l2->x = 235; l2->y = 165;
        l2->setType("square");
        l2->startWalking();
        l2->faceFrame = 6;
        l2->bounceVel = 0.22f;
        l2->loop = false;

        world->addPeep(l1);
        world->addPeep(l2);

        auto it = std::find_if(ctx->avoidSpots.begin(), ctx->avoidSpots.end(),
            [&](const Spot& s) { return s.x == spot.x && s.y == spot.y; });
        if (it != ctx->avoidSpots.end()) ctx->avoidSpots.erase(it);
        ctx->noYellingYet = false;
    }

    void byeSelf() {
        if (world) {
            HappyWeirdoPeep* happy = world->make<HappyWeirdoPeep>(ctx);
            world->addPeep(happy);
        }
        kill();
    }
};

struct ProtestAnim : AnimationProp {
    int triples = 0, mode = 0;

    explicit ProtestAnim(SceneCtx* c) : AnimationProp(c) {
        cls = "ProtestAnim";
        init(650, 200, "peace");
        c->avoidSpots.push_back(Spot{640, 150, 150});
    }

    void updateAnimation() override {
        triples = (triples + 1) % 3;
        if (triples != 0) return;
        if (mode == 0) {
            if (frame < 30) ++frame;
            else frame = 0;
        } else {
            frame = 31;
        }
    }

    void beStunned() { mode = 1; }
};

// ---------------------------------------------------------------- post-credits props
struct Candlelight : Actor {
    int frame = 0;
    Candlelight(SceneCtx*, float px, float py) {
        cls = "Candlelight";
        x = px - 100;
        y = py - 100;
    }
    void update() override {
        if (randf() < 0.2f) frame = int(randf() * 5);
    }
    void draw() override {
        SDL_Rect f = spriteFrame("candlelight", frame);
        // Candlelight.js anchors the movieclip at 0.5,0.5.
        drawTex(T("candlelight"), &f, x + f.w * .5f, y + f.h * .5f, float(f.w), float(f.h));
    }
};

struct LoversWatching : Actor {
    int frame = 0;
    float breathe = 0, breatheSpeed = 0;
    LoversWatching(SceneCtx*, const char* type) {
        cls = "LoversWatching";
        if (!std::strcmp(type, "circle")) { x = 520; y = 380; breatheSpeed = 0.025f; frame = 0; }
        else { x = 550; y = 375; breatheSpeed = 0.020f; frame = 1; }
    }
    void update() override { breathe += breatheSpeed; }
    void draw() override {
        SDL_Rect f = spriteFrame("lovers_watching", frame);
        float sc = 1 + std::sin(breathe) * 0.02f;
        float w = f.w * sc, h = f.h / sc;
        // MovieClip anchor is 0,0 in LoversWatching.js: top-left at (x, y).
        drawTex(T("lovers_watching"), &f, x + w * .5f, y + h * .5f, w, h);
    }
};

struct ImageProp : Actor {
    SDL_Texture* tex = nullptr;
    ImageProp(SceneCtx*, SDL_Texture* t, float px, float py, float w, float h) {
        cls = "ImageProp"; tex = t; x = px; y = py; width = w; height = h;
    }
    void draw() override { drawTex(tex, nullptr, x + width * .5f, y + height * .5f, width, height); }
};

// ---------------------------------------------------------------- GameScene (Scene_Game.js)
struct GameScene {
    SceneCtx ctx;
    World world;
    Camera camera;
    TV tv;
    Director director;
    ScreenShake shaker;
    ScreenZoomOut zoomer;

    float scale = 1, sx = 0, sy = 0, offX = 0, offY = 0;
    bool postCredits = false;
    bool wantCredits = false, wantPostPost = false;
    int fadeTicks = 0;
    ImageProp* stream = nullptr;
    vector<std::function<void()>> pendingActions;
    // Act II/III state that the original kept on Game (the JS globals). Plain
    // members, not shared_ptr: a reference-count increment costs an atomic
    // read-modify-write, and the first one of those in the program livelocks on
    // this target (stwcx. never commits), which is what froze the game when the
    // snobby cut spawned the protest handler.
    ProtestAnim* protestAnim = nullptr;
    int weaponIndex = 0;

    void defer(std::function<void()> f) { pendingActions.push_back(std::move(f)); }

    explicit GameScene(bool pc) {
        postCredits = pc;
        ctx.world = &world;
        ctx.camera = &camera;
        ctx.tv = &tv;
        ctx.director = &director;
        ctx.shaker = &shaker;
        world.init(&ctx, pc ? "bg_dark" : "bg");
        camera.noIntro = pc;
        camera.streaming = pc;
        camera.init(&ctx);
        director.init(&ctx);
        fadeTicks = sec60(1.f);
        if (pc) initPostCredits(); else initPlay();
    }

    void initPlay() {
        world.addProp(&tv);
        stageStart();
        stageHat();
    }

    void initPostCredits() {
        world.shadeAlpha = 79.f / 255.f; // bg_shade.png mean alpha
        stream = world.make<ImageProp>(&ctx, nullptr, 418.f, 238.25f, 120.f, 67.5f);
        world.addBG(stream);
        camera.x = W;
        camera.y = H;
        camera.onTakePhoto = [this] { wantPostPost = true; };

        static const float candlePos[18][2] = {
            {468.6f,276.6f},{535.7f,281.6f},{679.7f,279.1f},{612,281.6f},{490.1f,314.1f},
            {421.8f,309},{363.1f,301.1f},{786.9f,304.4f},{726.1f,310.5f},{656.9f,309.5f},
            {869.8f,350.3f},{820,373.5f},{768.2f,382.8f},{698.4f,389},{464.6f,386.1f},
            {396.3f,382.3f},{339.6f,370.1f},{294.3f,350.3f}};
        for (auto& p : candlePos)
            world.addBG(world.make<Candlelight>(&ctx, p[0], p[1]));

        world.addProp(world.make<LoversWatching>(&ctx, "circle"));
        world.addProp(world.make<LoversWatching>(&ctx, "square"));

        static const float cricketPos[3][2] = {{400, 353}, {420, 370}, {450, 380}};
        for (auto& p : cricketPos) {
            Cricket* c = world.make<Cricket>(&ctx);
            c->x = p[0]; c->y = p[1]; c->frame = 1;
            world.addProp(c);
        }
    }

    void update() {
        if (fadeTicks > 0) --fadeTicks;
        if (postCredits) {
            world.update();
            camera.update();
            if (stream) stream->tex = camera.getTexture();
            return;
        }
        world.update();
        ckpt("g.world");
        camera.update();
        ckpt("g.camera");
        director.update();
        ckpt("g.director");
        if (!pendingActions.empty()) {
            auto acts = std::move(pendingActions);
            pendingActions.clear();
            for (auto& f : acts) if (f) f();
        }
        ckpt("g.pending");
        zoomer.update(scale, sx, sy);
        shaker.update(scale);
        offX = shaker.offX;
        offY = shaker.offY;
        float ratio = zoomer.timer / float(zoomer.fullTimer);
        ratio = 1 - ratio;
        shaker.baseAlpha = 0.15f + ratio * 0.45f;
    }

    void draw() {
        // World -> world texture, like World.graphics pivot/scale.
        SDL_SetRenderTarget(g_rend, g_worldTex);
        // Explicit clip: at the 8x TV cut every sprite's destination is far
        // outside the target, so let the backend scissor instead of rasterising
        // huge quads.
        SDL_Rect worldClip{0, 0, int(W), int(H)};
        SDL_RenderSetClipRect(g_rend, &worldClip);
        SDL_SetRenderDrawColor(g_rend, 0, 0, 0, 255);
        SDL_RenderClear(g_rend);
        g_view.s = world.viewScale;
        g_view.tx = 480.f - world.pivotX * world.viewScale;
        g_view.ty = 270.f - world.pivotY * world.viewScale;
        ckpt("d.world.begin");
        world.draw();
        ckpt("d.world");
        SDL_RenderSetClipRect(g_rend, nullptr);
        g_view = View();

        // Scene texture: world, camera HUD, snow, laptop.
        SDL_SetRenderTarget(g_rend, g_sceneTex);
        SDL_SetRenderDrawColor(g_rend, 0, 0, 0, 255);
        SDL_RenderClear(g_rend);
        SDL_RenderCopy(g_rend, g_worldTex, nullptr, nullptr);
        camera.draw();
        shaker.draw();
        SDL_SetRenderTarget(g_rend, nullptr);

        SDL_SetRenderDrawColor(g_rend, 0, 0, 0, 255);
        SDL_RenderClear(g_rend);
        SDL_Rect dst{int(std::lround(sx + offX)), int(std::lround(sy + offY)),
                     int(std::lround(W * scale)), int(std::lround(H * scale))};
        SDL_RenderCopy(g_rend, g_sceneTex, nullptr, &dst);
        // ScreenZoomOut's laptop belongs to the scene's coordinate space, but its
        // art is 2592x1459 around the 960x540 screen: almost all of it lies
        // outside the scene frame. Drawn into the scene texture it was clipped
        // away entirely, which is why the laptop never showed up at the ending.
        // Draw it on the window pass instead, in scene space, so its frame wraps
        // the shrunken game. The screen shake cancels out (ScreenZoomOut.fixLaptop).
        if (!postCredits && zoomer.started && scale > 0.f && T("laptop")) {
            SDL_Rect lap{int(std::lround(sx - 816.f * scale)), int(std::lround(sy - 459.f * scale)),
                         int(std::lround(2592.f * scale)), int(std::lround(1459.f * scale))};
            SDL_RenderCopy(g_rend, T("laptop"), nullptr, &lap);
        }
        ckpt("d.scene");

        if (fadeTicks > 0) {
            float fa = 1.f - easeApply(Ease::QuadInOut, 1.f - fadeTicks / 60.f);
            SDL_SetTextureAlphaMod(T("quote0001"), Uint8(std::clamp(fa, 0.f, 1.f) * 255.f));
            SDL_RenderCopy(g_rend, T("quote0001"), nullptr, nullptr);
            SDL_SetTextureAlphaMod(T("quote0001"), 255);
        }
    }

    // ---- stages (Act_I/II/III) ----
    void stageStart();
    void stageHat();
    void stageLovers();
    void stageScreamer();
    void stageNervous();
    void stageSnobby();
    void stageAngry();
    void stageEvil();
    void stagePanic();
    void freezeEveryone();

    // ---- declarative chyron/cut modules ----
    bool chyLovers(Director& d);
    bool chyHats(Director& d);
    void chyPeeps(Director& d);
    bool chyAngry(Director& d);
    bool chyHelping(Director& d);
    bool chyProtest(Director& d);
    bool chyWeirdo(Director& d);
    bool chyShocked(Director& d);

    bool cutLovers(Director& d);
    bool cutHats(Director& d);
    bool cutPeeps(Director& d);
    bool cutAngry(Director& d);

    void replaceWatcher(const char* type, Peep* newPeep);
};

// ---------------------------------------------------------------- Acts I & II
int g_manifestoIndex = -1;
string spoutManifesto() {
    static const char* lines[] = {
        "who tunes in to watch *people get along?*",
        "peace is boring. violence goes viral.",
        "and every story needs a conflict, so...",
        "GIVE THE AUDIENCE WHAT THEY WANT."};
    if (g_manifestoIndex < 3) ++g_manifestoIndex;
    if (g_manifestoIndex < 0) g_manifestoIndex = 0;
    return lines[g_manifestoIndex];
}

void GameScene::replaceWatcher(const char* type, Peep* newPeep) {
    vector<NormalPeep*> watchers;
    for (Actor* a : world.peeps)
        if (!std::strcmp(a->cls, "NormalPeep")) {
            NormalPeep* p = static_cast<NormalPeep*>(a);
            if (p->type == type && p->isWatching) watchers.push_back(p);
        }
    if (watchers.empty()) {
        world.addPeep(newPeep);
        newPeep->watchTV();
        return;
    }
    NormalPeep* oldPeep = watchers[int(randf() * float(watchers.size()))];
    newPeep->x = oldPeep->x;
    newPeep->y = oldPeep->y;
    if (std::find(world.peeps.begin(), world.peeps.end(), static_cast<Actor*>(newPeep)) == world.peeps.end())
        world.addPeep(newPeep);
    oldPeep->kill();
    newPeep->watchTV();
}

bool GameScene::chyLovers(Director& d) {
    auto lovers = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "LoverPeep"); });
    if (lovers.empty()) return false;
    LoverPeep* l = static_cast<LoverPeep*>(lovers[0]);
    if (l->isEmbarrassed) {
        d.chyron = "yeah git on outta here";
    } else {
        d.data.caughtLovers = true;
        d.data.forceChyron = true;
        d.chyron = "GROSS, GO GET A ROOM";
    }
    return true;
}

bool GameScene::chyHats(Director& d) {
    auto hats = d.inCamera([](Actor* a) {
        return !std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->wearingHat;
    });
    if (hats.empty()) return false;
    d.data.audience = 1;
    d.data.caughtHat = true;
    d.chyron = "nvm hats aren't cool anymore";
    return true;
}

void GameScene::chyPeeps(Director& d) {
    if (camera.isOverTV(true)) {
        d.chyron = "A TV... ON TV!";
        return;
    }
    auto peepsIn = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "NormalPeep"); });
    auto crickets = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "Cricket"); });
    if (!crickets.empty()) {
        d.data.CAUGHT_A_CRICKET = true;
        d.chyron = crickets.size() == 1 ? "LIL' CRICKY <3" : "okay that's too many crickets";
    } else if (!peepsIn.empty()) {
        d.chyron = peepsIn.size() == 1 ? "just a normal peep" : "just some normal peeps";
    } else {
        d.data.ITS_NOTHING = true;
        d.chyron = "WOWWEE, IT'S NOTHING";
    }
}

bool GameScene::chyAngry(Director& d) {
    auto angry = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "AngryPeep"); });
    if (angry.empty()) return false;
    auto circleShouting = d.inCamera([](Actor* a) {
        return !std::strcmp(a->cls, "AngryPeep") &&
               static_cast<AngryPeep*>(a)->type == "circle" && static_cast<AngryPeep*>(a)->isShouting;
    });
    auto squareShouting = d.inCamera([](Actor* a) {
        return !std::strcmp(a->cls, "AngryPeep") &&
               static_cast<AngryPeep*>(a)->type == "square" && static_cast<AngryPeep*>(a)->isShouting;
    });
    auto shocked = d.inCamera([](Actor* a) {
        return !std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->shocked;
    });

    if (!circleShouting.empty() || !squareShouting.empty()) {
        d.data.audience = 2;
        d.data.caughtAngry = true;
        int angryCount = 0;
        for (Actor* a : world.peeps) if (!std::strcmp(a->cls, "AngryPeep")) ++angryCount;
        float ratio = float(angryCount + 4) / float(std::max<int>(1, int(world.peeps.size()) - 1));
        if (ratio >= 1.f) d.chyron = "EVERYONE HATES EVERYONE!!1!";
        else if (ratio >= 0.75f) d.chyron = "ALMOST EVERYONE HATES EVERYONE...";
        else {
            if (!circleShouting.empty()) d.data.caughtAngryCircle = true;
            if (!squareShouting.empty()) d.data.caughtAngrySquare = true;
            d.chyron = circleShouting.empty() ? "SQUARES HATE CIRCLES" : "CIRCLES HATE SQUARES";
        }
    } else {
        d.chyron = !shocked.empty() ? "oooooh just missed it" : "(ya gotta catch 'em *yelling* at others)";
    }
    (void)angry;
    return true;
}

bool GameScene::chyHelping(Director& d) {
    auto helping = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "HelpingAnim"); });
    if (helping.empty()) return false;
    d.chyron = static_cast<HelpingAnim*>(helping[0])->hasHelped ? spoutManifesto()
                                                               : "what are these nerds doing now";
    return true;
}

bool GameScene::chyProtest(Director& d) {
    auto protest = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "ProtestAnim"); });
    if (protest.empty()) return false;
    d.chyron = spoutManifesto();
    return true;
}

bool GameScene::chyWeirdo(Director& d) {
    auto weirdo = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "HappyWeirdoPeep"); });
    if (!weirdo.empty()) { d.chyron = spoutManifesto(); return true; }
    auto lovers = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "LoverPeep"); });
    if (!lovers.empty()) { d.chyron = spoutManifesto(); return true; }
    return false;
}

bool GameScene::chyShocked(Director& d) {
    auto shocked = d.inCamera([](Actor* a) {
        return !std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->shocked;
    });
    if (shocked.empty()) return false;
    d.chyron = "why's this peep shocked?";
    return true;
}

bool GameScene::cutLovers(Director& d) {
    if (!d.data.caughtLovers) return false;
    d.audienceCutToTV();
    vector<Actor*> copy = world.peeps;
    for (Actor* a : copy)
        if (!std::strcmp(a->cls, "LoverPeep")) static_cast<LoverPeep*>(a)->makeEmbarrassed();
    return true;
}

bool GameScene::cutHats(Director& d) {
    if (d.data.caughtHat) {
        d.audienceCutToTV([](Peep* p) { static_cast<NormalPeep*>(p)->takeOffHat(false); },
                          [](Peep* p) { return static_cast<NormalPeep*>(p)->wearingHat; });
        return true;
    }
    vector<NormalPeep*> hatPeeps;
    for (Actor* a : world.peeps)
        if (!std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->wearingHat)
            hatPeeps.push_back(static_cast<NormalPeep*>(a));
    if (!hatPeeps.empty()) hatPeeps[int(randf() * float(hatPeeps.size()))]->takeOffHat(true);
    return false;
}

bool GameScene::cutPeeps(Director& d) {
    d.audienceCutToTV();
    return true;
}

bool GameScene::cutAngry(Director& d) {
    if (!d.data.caughtAngry) return false;
    d.audienceCutToTV();

    vector<NormalPeep*> watchers;
    for (Actor* a : world.peeps)
        if (!std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->isWatching)
            watchers.push_back(static_cast<NormalPeep*>(a));

    for (NormalPeep* peep : watchers) {
        if (peep->dead) continue;
        AngryPeep* np = world.make<AngryPeep>(&ctx, peep->type);
        np->x = peep->x;
        np->y = peep->y;
        world.addPeep(np);
        peep->kill();
        np->watchTV();
    }
    return true;
}

void GameScene::stageStart() {
    world.clearPeeps();
    world.addBalancedPeeps(20);
}

void GameScene::stageHat() {
    HatPeep* hat = world.make<HatPeep>(&ctx);
    world.addPeep(hat);

    director.cbTakePhoto = [this](Director& d) {
        auto hats = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "HatPeep"); });
        if (!hats.empty()) {
            d.data.audience = 3;
            d.data.caughtHatProp = hats[0];
            d.data.caughtHat = true;
            d.chyron = "OOH NICE HAT";
        } else {
            chyPeeps(d);
        }
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    director.cbCutToTV = [this](Director& d) {
        if (d.data.caughtHatProp) {
            world.addBalancedPeeps(1);
            d.audienceCutToTV([](Peep* p) { static_cast<NormalPeep*>(p)->wearHat(); });
            if (!d.data.caughtHatProp->dead) d.data.caughtHatProp->kill();
            defer([this] { stageLovers(); });
        } else {
            d.audienceCutToTV();
        }
    };
}

void GameScene::stageLovers() {
    LoverPeep* lover1 = world.make<LoverPeep>(&ctx);
    lover1->setType("circle");
    LoverPeep* lover2 = world.make<LoverPeep>(&ctx);
    lover2->setType("square");
    lover2->follow(lover1);
    world.addPeep(lover1);
    world.addPeep(lover2);

    director.cbTakePhoto = [this](Director& d) {
        if (chyLovers(d)) return;
        if (chyHats(d)) return;
        chyPeeps(d);
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    director.cbCutToTV = [this](Director& d) {
        if (!cutLovers(d)) {
            if (!cutHats(d)) cutPeeps(d);
        }
        defer([this] { stageScreamer(); });
    };
}

void GameScene::stageScreamer() {
    CrazyPeep* crazy = world.make<CrazyPeep>(&ctx);
    world.addPeep(crazy);

    director.cbTakePhoto = [this](Director& d) {
        auto crazies = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "CrazyPeep"); });
        if (!crazies.empty()) {
            CrazyPeep* c = static_cast<CrazyPeep*>(crazies[0]);
            if (c->isScreaming()) {
                d.data.HIJACK = true;
                d.data.audienceCircles = 1;
                d.data.audienceSquares = 0;
                d.data.caughtCrazyProp = crazies[0];
                d.chyron = "CRAZED SQUARE ATTACKS";
            } else {
                auto shocked = d.inCamera([](Actor* a) {
                    return !std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->shocked;
                });
                d.chyron = !shocked.empty() ? "oooooh just missed it"
                                            : "(ya gotta catch 'em doing *something* interesting...)";
            }
            return;
        }
        if (chyLovers(d)) return;
        if (chyHats(d)) return;
        auto shocked = d.inCamera([](Actor* a) {
            return !std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->shocked;
        });
        if (!shocked.empty()) { d.chyron = "(ya gotta catch who's screaming at 'em)"; return; }
        chyPeeps(d);
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    director.cbCutToTV = [this](Director& d) {
        if (d.data.caughtCrazyProp) {
            world.addBalancedPeeps(1);
            d.audienceCutToTV();
            if (!d.data.caughtCrazyProp->dead) d.data.caughtCrazyProp->kill();
            NervousPeep* nervous = world.make<NervousPeep>(&ctx);
            replaceWatcher("circle", nervous);
            defer([this] { stageNervous(); });
        } else if (!cutLovers(d)) {
            if (!cutHats(d)) cutPeeps(d);
        }
    };
}

void GameScene::stageNervous() {
    vector<Actor*> copy = world.peeps;
    for (Actor* a : copy)
        if (!std::strcmp(a->cls, "LoverPeep")) a->kill();

    director.cbTakePhoto = [this](Director& d) {
        auto nervous = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "NervousPeep"); });
        auto confused = d.inCamera([](Actor* a) {
            return !std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->confused;
        });
        if (!nervous.empty()) {
            NervousPeep* n = static_cast<NervousPeep*>(nervous[0]);
            if (n->isScared()) {
                if (!confused.empty()) {
                    d.data.HIJACK = true;
                    d.data.audienceCircles = 0;
                    d.data.audienceSquares = 1;
                    d.data.caughtNervousProp = nervous[0];
                    d.chyron = "CIRCLE FEARS SQUARES";
                } else {
                    d.chyron = "(ya gotta also catch *who* they're scared by)";
                }
            } else {
                d.chyron = "(ya gotta catch 'em doing *something* interesting...)";
            }
            return;
        }
        if (chyHats(d)) return;
        chyPeeps(d);
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    director.cbCutToTV = [this](Director& d) {
        if (d.data.caughtNervousProp) {
            world.addBalancedPeeps(1);
            d.audienceCutToTV();
            if (!d.data.caughtNervousProp->dead) d.data.caughtNervousProp->kill();
            SnobbyPeep* snobby = world.make<SnobbyPeep>(&ctx);
            replaceWatcher("square", snobby);
            defer([this] { stageSnobby(); });
        } else if (!cutHats(d)) {
            cutPeeps(d);
        }
    };
}

void GameScene::stageSnobby() {
    director.cbTakePhoto = [this](Director& d) {
        auto snobby = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "SnobbyPeep"); });
        if (!snobby.empty()) {
            SnobbyPeep* s = static_cast<SnobbyPeep*>(snobby[0]);
            if (s->isSmug) {
                d.data.HIJACK = true;
                d.data.audienceCircles = 1;
                d.data.audienceSquares = 0;
                d.data.caughtSnobbyProp = snobby[0];
                d.chyron = "SQUARES SNUB CIRCLES";
            } else {
                d.chyron = "(ya gotta catch 'em doing *something* interesting...)";
            }
            return;
        }
        if (chyHats(d)) return;
        chyPeeps(d);
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    director.cbCutToTV = [this](Director& d) {
        if (d.data.caughtSnobbyProp) {
            ckpt("cut.snobby");
            vector<Actor*> copy = world.peeps;
            for (Actor* a : copy)
                if (!std::strcmp(a->cls, "NormalPeep") && static_cast<NormalPeep*>(a)->wearingHat)
                    static_cast<NormalPeep*>(a)->takeOffHat(true);

            d.audienceCutToTV();
            if (!d.data.caughtSnobbyProp->dead) d.data.caughtSnobbyProp->kill();
            AngryPeep* angry = world.make<AngryPeep>(&ctx, "circle");
            replaceWatcher("circle", angry);
            defer([this] { stageAngry(); });
        } else if (!cutHats(d)) {
            cutPeeps(d);
        }
    };
}

// ---------------------------------------------------------------- Act II finale & Act III
void GameScene::stageAngry() {
    ckpt("stage.angry");
    HelpingAnim* helping = world.make<HelpingAnim>(&ctx);
    ckpt("sa.made");
    world.addProp(helping);
    ckpt("sa.prop");

    auto addProtesters = [this]() {
        if (protestAnim) return;
        ProtestAnim* p = world.make<ProtestAnim>(&ctx);
        world.addProp(p);
        protestAnim = p;
    };

    ckpt("sa.lambda");
    director.cbTakePhoto = [this](Director& d) {
        if (chyHelping(d)) return;
        if (chyProtest(d)) return;
        if (chyAngry(d)) return;
        if (chyWeirdo(d)) return;
        if (chyShocked(d)) return;
        chyPeeps(d);
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    ckpt("sa.cb2");
    director.cbCutToTV = [this, addProtesters](Director& d) {
        if (!cutAngry(d)) cutPeeps(d);

        int angry = 0;
        for (Actor* a : world.peeps) if (!std::strcmp(a->cls, "AngryPeep")) ++angry;
        float ratio = float(angry) / float(std::max<int>(1, int(world.peeps.size()) - 1));
        if (ratio > 0.5f) addProtesters();
        if (ratio >= 1.0f) defer([this] { stageEvil(); });
    };
}

void GameScene::freezeEveryone() {
    audioStop(g_parkVoice, 0.5f);
    vector<Peep*> freezeEm;
    for (Actor* a : world.peeps)
        if (!std::strcmp(a->cls, "NormalPeep") || !std::strcmp(a->cls, "AngryPeep"))
            freezeEm.push_back(static_cast<Peep*>(a));

    vector<Peep*> copy = freezeEm;
    for (Peep* oldPeep : copy) {
        if (oldPeep->dead) continue;
        NormalPeep* stunned = world.make<NormalPeep>(&ctx);
        stunned->setType(oldPeep->shapeType());
        stunned->velX = oldPeep->velX;
        stunned->velY = oldPeep->velY;
        stunned->flip = (oldPeep->x < W * .5f) ? 1 : -1;
        stunned->beStunned();
        stunned->x = oldPeep->x;
        stunned->y = oldPeep->y;
        if (std::find(world.peeps.begin(), world.peeps.end(), static_cast<Actor*>(stunned)) == world.peeps.end())
            world.addPeep(stunned);
        oldPeep->kill();
        stunned->update(); // JUST. IN. CASE.
    }

    for (Actor* a : world.props) {
        if (!std::strcmp(a->cls, "ProtestAnim")) {
            static_cast<ProtestAnim*>(a)->beStunned();
            break;
        }
    }
}

void GameScene::stageEvil() {
    HappyWeirdoPeep* happy = nullptr;
    for (Actor* a : world.peeps)
        if (!std::strcmp(a->cls, "HappyWeirdoPeep")) { happy = static_cast<HappyWeirdoPeep*>(a); break; }
    if (!happy) {
        happy = world.make<HappyWeirdoPeep>(&ctx);
        world.addPeep(happy);
    }
    happy->prepareForMurder();

    EvilHatPeep* murderer = world.make<EvilHatPeep>(&ctx);
    murderer->victim = happy;
    murderer->freezeEveryone = [this] {
        audioPlay("gun_cock", false, 1.f);
        defer([this] { freezeEveryone(); });
    };
    murderer->bang = [this] {
        audioPlay("gunshot", false, 1.f);
        defer([this] { stagePanic(); });
    };
    world.addPeep(murderer);

    director.cbTakePhoto = [this, murderer](Director& d) {
        if (murderer->hasGunOut) {
            d.chyron = ". . .";
            return;
        }
        auto evil = d.inCamera([](Actor* a) { return !std::strcmp(a->cls, "EvilHatPeep"); });
        if (!evil.empty()) {
            d.chyron = "hats ain't cool no more, haven't you heard?";
            return;
        }
        if (chyProtest(d)) return;
        if (chyAngry(d)) return;
        if (chyWeirdo(d)) return;
        if (chyShocked(d)) return;
        chyPeeps(d);
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    director.cbCutToTV = [](Director& d) { d.audienceCutToTV(); };
}

void GameScene::stagePanic() {
    camera.noSounds = true;
    director.noSounds = true;
    g_panicVoice = audioPlay("bg_panic", true, 0.75f);
    g_creepyVoice = audioPlay("bg_creepy", true, 0.f);
    audioSetVolume(g_creepyVoice, 1.f, 5.f);
    shakeScreen(&ctx, 100);

    ctx.avoidSpots.clear();
    ctx.avoidSpots.push_back(Spot{530, 430, 150});

    // Everyone becomes a panic peep.
    vector<NormalPeep*> normalPeeps;
    for (Actor* a : world.peeps)
        if (!std::strcmp(a->cls, "NormalPeep")) normalPeeps.push_back(static_cast<NormalPeep*>(a));
    for (NormalPeep* oldPeep : normalPeeps) {
        if (oldPeep->dead) continue;
        PanicPeep* panicPeep = world.make<PanicPeep>(&ctx);
        panicPeep->setType(oldPeep->type);
        panicPeep->x = oldPeep->x;
        panicPeep->y = oldPeep->y;
        world.addPeep(panicPeep);
        oldPeep->kill();
    }

    // Protest -> panicked lovers.
    for (Actor* a : world.props) {
        if (!std::strcmp(a->cls, "ProtestAnim")) { a->kill(); break; }
    }
    PanicPeep* panicCircle = world.make<PanicPeep>(&ctx);
    panicCircle->setLover("circle");
    world.addPeep(panicCircle);
    PanicPeep* panicSquare = world.make<PanicPeep>(&ctx);
    panicSquare->setLover("square");
    world.addPeep(panicSquare);

    // Explode the happy one.
    HappyWeirdoPeep* happy = nullptr;
    for (Actor* a : world.peeps)
        if (!std::strcmp(a->cls, "HappyWeirdoPeep")) { happy = static_cast<HappyWeirdoPeep*>(a); break; }

    if (happy) {
        DeadBody* deadbody = world.make<DeadBody>(&ctx);
        deadbody->init(-1.4f, 3.5f, happy->x, happy->y, -10, -1, 0);
        world.addProp(deadbody);
        for (int i = 0; i < 30; ++i) {
            Gore* gore = world.make<Gore>(&ctx);
            gore->init(-(float)TAU / 4.f + randf(), 10.f + randf() * 5.f,
                       happy->x, happy->y, randf() * -30.f);
            world.addProp(gore);
        }
        happy->kill();
    }

    zoomer.onComplete = [this] {
        audioStop(g_creepyVoice, 0.4f);
        audioStop(g_panicVoice, 0.4f);
        wantCredits = true;
    };

    director.cbTakePhoto = [](Director& d) {
        d.chyron = "BE SCARED. BE ANGRY.";
        d.data.forceChyron = true;
    };
    director.cbMovePhoto = [](Director& d) { d.audienceMovePhoto(); };
    director.cbCutToTV = [this](Director& d) {
        d.audienceCutToTV();

        for (Actor* a : world.peeps)
            if (!std::strcmp(a->cls, "EvilHatPeep")) { a->kill(); ctx.avoidSpots.clear(); break; }

        vector<Actor*> murderers;
        for (Actor* a : world.peeps)
            if (!std::strcmp(a->cls, "MurderPeep")) murderers.push_back(a);
        for (Actor* a : murderers) a->kill();
        if (!murderers.empty() && !zoomer.started) zoomer.init();

        static const char* weapons[] = {"gun", "bat", "shotgun", "axe"};
        int wi = weaponIndex;
        MurderPeep* m1 = world.make<MurderPeep>(&ctx);
        m1->init("circle", weapons[wi]);
        world.addPeep(m1);
        MurderPeep* m2 = world.make<MurderPeep>(&ctx);
        m2->init("square", weapons[(wi + 1) % 4]);
        world.addPeep(m2);
        weaponIndex = (wi + 1) % 4;
    };
}

// ---------------------------------------------------------------- scenes
enum class SceneId { Quote, Game, Credits, PostCredits, PostPost };

SceneId g_scene = SceneId::Quote;
std::unique_ptr<GameScene> g_game;
int g_sceneTicks = 0;
float g_cursorX = 480, g_cursorY = 270;
// The "Ported by PortBREW" badge shows until the first photo is taken.
bool g_hasCaptured = false;
int g_clickTicks = 0;
int g_windowW = 1280, g_windowH = 720;
std::vector<SDL_GameController*> g_pads;
std::vector<SDL_Joystick*> g_sticks;

void openInput(int deviceIndex) {
    if (SDL_IsGameController(deviceIndex)) {
        SDL_GameController* c = SDL_GameControllerOpen(deviceIndex);
        if (!c) return;
        SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
        for (SDL_GameController* p : g_pads)
            if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p)) == id) {
                SDL_GameControllerClose(c);
                return;
            }
        g_pads.push_back(c);
        return;
    }
    SDL_Joystick* js = SDL_JoystickOpen(deviceIndex);
    if (!js) return;
    SDL_JoystickID id = SDL_JoystickInstanceID(js);
    for (SDL_Joystick* p : g_sticks)
        if (SDL_JoystickInstanceID(p) == id) {
            SDL_JoystickClose(js);
            return;
        }
    g_sticks.push_back(js);
}

void closeInput(SDL_JoystickID instance) {
    for (auto it = g_pads.begin(); it != g_pads.end(); ++it) {
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*it)) == instance) {
            SDL_GameControllerClose(*it);
            g_pads.erase(it);
            return;
        }
    }
    for (auto it = g_sticks.begin(); it != g_sticks.end(); ++it) {
        if (SDL_JoystickInstanceID(*it) == instance) {
            SDL_JoystickClose(*it);
            g_sticks.erase(it);
            return;
        }
    }
}

void gotoScene(SceneId id) {
    g_game.reset();
    g_scene = id;
    g_sceneTicks = 0;
    g_clickTicks = 0;
    if (id == SceneId::Game) g_game = std::make_unique<GameScene>(false);
    if (id == SceneId::PostCredits) g_game = std::make_unique<GameScene>(true);
}

void setCursor(float cx, float cy) {
    g_cursorX = std::clamp(cx, 0.f, W);
    g_cursorY = std::clamp(cy, 0.f, H);
    if (g_game && (g_scene == SceneId::Game || g_scene == SceneId::PostCredits)) {
        g_game->camera.x = g_cursorX;
        g_game->camera.y = g_cursorY;
    }
}

void postPostButtonRect(int i, float& cx, float& cy, float& hw, float& hh) {
    static const float xs[3] = {250, 480, 710};
    cx = xs[i];
    cy = 325;
    hw = 207.f * DRAWSCALE * .5f;
    hh = 57.f * DRAWSCALE * .5f;
}

void clickAtCursor() {
    static Uint32 lastClick = 0;
    Uint32 now = SDL_GetTicks();
    if (lastClick && now - lastClick < 120) return; // one press = one click
    lastClick = now;
    g_clickTicks = 8;
    if (g_scene == SceneId::Game && g_game) {
        if (!g_game->camera.frozen) {
            g_hasCaptured = true;
            g_game->camera.takePhoto();
            g_game->camera.frozen = true; // Camera.js freezes for the whole sequence
            g_game->director.takePhoto(&g_game->camera);
        }
    } else if (g_scene == SceneId::PostCredits && g_game) {
        if (!g_game->camera.frozen) {
            audioStop(g_nightVoice, 0.8f);
            g_game->camera.takePhoto();
        }
    } else if (g_scene == SceneId::PostPost) {
        if (g_sceneTicks < 120) return;
        for (int i = 0; i < 3; ++i) {
            float cx, cy, hw, hh;
            postPostButtonRect(i, cx, cy, hw, hh);
            if (std::fabs(g_cursorX - cx) < hw && std::fabs(g_cursorY - cy) < hh) {
                audioPlay("squeak", false, 1.f);
                if (i == 2) gotoScene(SceneId::Quote);
                return;
            }
        }
    }
}

// ---------------------------------------------------------------- watermark
// Small "Ported by PortBREW" badge in the bottom-right corner: the logo, then the
// text. It is drawn in screen space (so the camera shake and the 8x TV cut cannot
// move it) and only until the player takes their first photo, so it never sits on
// top of a capture.
void drawWatermark() {
    if (g_hasCaptured) return;
    const string label = "Ported by PortBREW";
    const float px = 11.f;                 // text height
    const float logoH = 16.f, logoW = 16.f, gap = 7.f;
    const float padX = 6.f, padY = 5.f;
    const float tw = textWidth(label, px);
    const float boxW = padX * 2.f + logoW + gap + tw;
    const float boxH = logoH + padY * 2.f;
    const float boxX = W - 10.f - boxW, boxY = H - 10.f - boxH;

    const View saved = g_view;
    g_view = View();                       // 960x540 screen space, not world space
    SDL_SetRenderTarget(g_rend, nullptr);
    fillRect(boxX, boxY, boxW, boxH, Col{0, 0, 0, 105});
    drawTex(T("portbrew"), nullptr, boxX + padX + logoW * .5f, boxY + boxH * .5f, logoW, logoH);
    textLine(label, boxX + padX + logoW + gap, boxY + (boxH - px) * .5f, px,
             Col{255, 255, 255, 235});
    g_view = saved;
}

void drawQuoteScene() {
    float t = float(g_sceneTicks);
    g_view = View();
    drawTex(T("quote0001"), nullptr, 480, 270, 960, 540);

    float textAlpha = 1.f;
    if (t >= 660) textAlpha = 1.f - easeApply(Ease::QuadInOut, (t - 660.f) / 60.f);
    if (textAlpha <= 0.f) return;

    float a2 = t > 90 ? easeApply(Ease::QuadInOut, (t - 90.f) / 60.f) : 0.f;
    float a3 = t > 390 ? easeApply(Ease::QuadInOut, (t - 390.f) / 60.f) : 0.f;
    float a4 = t > 510 ? easeApply(Ease::QuadInOut, (t - 510.f) / 60.f) : 0.f;

    if (a2 > 0) {
        Uint8 al = Uint8(std::clamp(a2 * textAlpha, 0.f, 1.f) * 255.f);
        drawTex(T("quote0002"), nullptr, 480, 270, 960, 540, 0, false, a2 * textAlpha);
        textCentered("We become what we behold.", 486, 230 - 32.5f, 65, Col{255, 255, 255, al});
        textCentered("We shape our tools and then our tools shape us.", 486, 290 - 19.5f, 39,
                     Col{255, 255, 255, al});
    }
    if (a3 > 0) {
        Uint8 al = Uint8(std::clamp(a3 * textAlpha, 0.f, 1.f) * 255.f);
        drawTex(T("quote0003"), nullptr, 480, 270, 960, 540, 0, false, a3 * textAlpha);
        textCentered("Marshall McLuhan", 484, 346 - 16.5f, 33, Col{255, 255, 255, al});
    }
    if (a4 > 0) {
        Uint8 al = Uint8(std::clamp(a4 * textAlpha, 0.f, 1.f) * 255.f);
        drawTex(T("quote0004"), nullptr, 480, 270, 960, 540, 0, false, a4 * textAlpha);
        textCentered("(misattributed)", 484, 405 - 16.5f, 33, Col{255, 255, 255, al});
    }
}

void drawCreditsScene() {
    float t = float(g_sceneTicks);
    g_view = View();
    SDL_SetRenderDrawColor(g_rend, 0, 0, 0, 255);
    SDL_RenderClear(g_rend);

    auto ramp = [](float v, float start, float dur) {
        return easeApply(Ease::QuadInOut, (v - start) / dur);
    };
    float a[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    a[1] = t < 240 ? 0 : t < 300 ? ramp(t, 240, 60) : t < 480 ? 1 : t < 540 ? 1 - ramp(t, 480, 60) : 0;
    a[2] = t < 540 ? 0 : t < 600 ? ramp(t, 540, 60) : t < 780 ? 1 : t < 840 ? 1 - ramp(t, 780, 60) : 0;
    a[3] = t < 840 ? 0 : t < 900 ? ramp(t, 840, 60) : t < 1020 ? 1 : 0;
    a[4] = (t >= 1020 && t < 1140) ? 1 : 0;
    a[5] = (t >= 1140 && t < 1260) ? 1 : 0;
    a[6] = t < 1260 ? 0 : t < 1380 ? 1 : t < 1440 ? 1 - ramp(t, 1380, 60) : 0;
    a[7] = t < 1440 ? 0 : t < 1500 ? ramp(t, 1440, 60) : 1;
    a[8] = t < 1560 ? 0 : t < 1620 ? ramp(t, 1560, 60) : 1;
    float cont = t < 1860 ? 1 : t < 1920 ? 1 - ramp(t, 1860, 60) : 0;

    for (int i = 1; i <= 8; ++i) {
        if (a[i] <= 0) continue;
        char name[32];
        std::snprintf(name, sizeof(name), "credits%04d", i);
        drawTex(T(name), nullptr, 480, 270, 960, 540, 0, false, a[i] * cont);
    }

    auto textOn = [&](int card, const string& s, float x, float y, float px, bool center) {
        if (a[card] <= 0) return;
        Uint8 al = Uint8(std::clamp(a[card] * cont, 0.f, 1.f) * 255.f);
        if (center) textCentered(s, x, y - px * .5f, px, Col{255, 255, 255, al});
        else textLine(s, x, y - px * .5f, px, Col{255, 255, 255, al});
    };
    textOn(1, "created by", 240, 229, 46, false);
    textOn(1, "Nicky Case", 240, 299, 86, false);
    if (a[2] > 0) {
        Uint8 al = Uint8(std::clamp(a[2] * cont, 0.f, 1.f) * 255.f);
        string s = "many thanks to my playtesters:";
        textLine(s, 742 - textWidth(s, 44), 130 - 22, 44, Col{255, 255, 255, al});
    }
    for (int i = 3; i <= 6; ++i) textOn(i, "and to my patreon supporters:", 145, 80 + 20, 40, false);
    if (a[7] > 0) {
        Uint8 al = Uint8(std::clamp(a[7] * cont, 0.f, 1.f) * 255.f);
        string s = "and last but not least,";
        float ty = 490 - textWidth("thank YOU for playing!", 55) * .5f;
        textLine(s, ty + textWidth("thank YOU for playing!", 55) - textWidth(s, 37), 225 - 18, 37,
                 Col{255, 255, 255, al});
    }
    textOn(8, "thank YOU for playing!", 490, 295, 55, true);
}

static int g_postPostHover = -1;

void drawPostPostScene() {
    g_view = View();
    drawTex(T("quote0001"), nullptr, 480, 270, 960, 540);
    if (g_sceneTicks < 120) return;

    drawTex(T("logo"), nullptr, 480, 270, 960, 540);
    int hovered = -1;
    for (int i = 0; i < 3; ++i) {
        float cx, cy, hw, hh;
        postPostButtonRect(i, cx, cy, hw, hh);
        bool hover = std::fabs(g_cursorX - cx) < hw && std::fabs(g_cursorY - cy) < hh;
        if (hover) hovered = i;
        SDL_Rect bg = spriteFrame("end_button", hover ? 1 : 0);
        SDL_Rect label = spriteFrame("end_button", 2 + i);
        float sc = hover ? 1.05f : 1.f;
        drawTex(T("end_button"), &bg, cx, cy, bg.w * DRAWSCALE * sc, bg.h * DRAWSCALE * sc);
        drawTex(T("end_button"), &label, cx, cy, label.w * DRAWSCALE * sc, label.h * DRAWSCALE * sc);
    }
    if (hovered != g_postPostHover) {
        if (hovered >= 0) audioPlay("squeak", false, 1.f);
        g_postPostHover = hovered;
    }
    drawTex(T("facebook"), nullptr, 442, 419, 32, 32);
    drawTex(T("twitter"), nullptr, 493, 419, 32, 32);

    int frame = g_clickTicks > 0 ? 4 : 0;
    SDL_Rect cur = spriteFrame("cursor", frame);
    drawTex(T("cursor"), &cur, g_cursorX + 25, g_cursorY + 25, cur.w * .5f, cur.h * .5f);
}

void tick() {
    ++g_sceneTicks;
    ckpt("tick");
    audioTick();
    if (g_clickTicks > 0) --g_clickTicks;

    // Arcade-style pointer for every pad: sticks, plus the D-pad that a bare
    // Wii Remote has instead of an analog stick.
    const float cursorStep = 1.f / 60.f;
    for (SDL_GameController* pad : g_pads) {
        float dx = 0, dy = 0;
        Sint16 ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (std::abs(ax) > 8000) dx = ax / 32767.f;
        if (std::abs(ay) > 8000) dy = ay / 32767.f;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) dx = -1.f;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) dx = 1.f;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) dy = -1.f;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) dy = 1.f;
        if (dx != 0 || dy != 0)
            setCursor(g_cursorX + dx * 700.f * cursorStep, g_cursorY + dy * 700.f * cursorStep);
    }
    for (SDL_Joystick* js : g_sticks) {
        float dx = 0, dy = 0;
        if (SDL_JoystickNumAxes(js) >= 2) {
            Sint16 ax = SDL_JoystickGetAxis(js, 0);
            Sint16 ay = SDL_JoystickGetAxis(js, 1);
            if (std::abs(ax) > 8000) dx = ax / 32767.f;
            if (std::abs(ay) > 8000) dy = ay / 32767.f;
        }
        if (SDL_JoystickNumHats(js) > 0) {
            Uint8 h = SDL_JoystickGetHat(js, 0);
            if (h & SDL_HAT_LEFT) dx -= 1.f;
            if (h & SDL_HAT_RIGHT) dx += 1.f;
            if (h & SDL_HAT_UP) dy -= 1.f;
            if (h & SDL_HAT_DOWN) dy += 1.f;
        }
        if (dx != 0 || dy != 0)
            setCursor(g_cursorX + dx * 700.f * cursorStep, g_cursorY + dy * 700.f * cursorStep);
    }

    switch (g_scene) {
        case SceneId::Quote:
            if (g_sceneTicks == 570) {
                g_parkVoice = audioPlay("bg_park", true, 0.f);
                audioSetVolume(g_parkVoice, 1.f, 2.f);
            }
            if (g_sceneTicks >= 720) gotoScene(SceneId::Game);
            break;
        case SceneId::Game:
            if (g_game) {
                g_game->update();
                if (g_game->wantCredits) { gotoScene(SceneId::Credits); break; }
            }
            break;
        case SceneId::Credits:
            if (g_sceneTicks == 1800) {
                g_nightVoice = audioPlay("bg_nighttime", true, 0.f);
                audioSetVolume(g_nightVoice, 1.f, 2.f);
            }
            if (g_sceneTicks >= 1920) gotoScene(SceneId::PostCredits);
            break;
        case SceneId::PostCredits:
            if (g_game) {
                g_game->update();
                if (g_game->wantPostPost) { gotoScene(SceneId::PostPost); break; }
            }
            break;
        case SceneId::PostPost:
            break;
    }
}

void render() {
    ckpt("render");
    SDL_SetRenderTarget(g_rend, nullptr);
    SDL_SetRenderDrawColor(g_rend, 0, 0, 0, 255);
    SDL_RenderClear(g_rend);
    if (g_scene == SceneId::Quote) drawQuoteScene();
    else if (g_scene == SceneId::Credits) drawCreditsScene();
    else if (g_scene == SceneId::PostPost) drawPostPostScene();
    else if (g_game) g_game->draw();
    drawWatermark();
    ckpt("present");
    SDL_RenderPresent(g_rend);
    ckpt("frame");
}

} // namespace

int main(int, char**) {
    // Touch is handled through SDL_FINGER*; don't let SDL also synthesize mouse
    // events for it (those arrived in window space and double-mapped the cursor).
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) return 1;
    SDL_Window* window = SDL_CreateWindow("We Become What We Behold", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
    if (!window) return 2;
    g_rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_rend) g_rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_rend) return 3;
    SDL_RenderSetLogicalSize(g_rend, int(W), int(H));
    SDL_SetRenderDrawBlendMode(g_rend, SDL_BLENDMODE_BLEND);

    g_worldTex = SDL_CreateTexture(g_rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 960, 540);
    g_sceneTex = SDL_CreateTexture(g_rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 960, 540);

    for (int i = 0; i < SDL_NumJoysticks(); ++i) openInput(i);

    loadArt();
    audioInit();
    SDL_CreateThread(stallWatch, "stall", nullptr);
    gotoScene(SceneId::Quote);

    bool running = true;
    double accumulator = 0;
    Uint64 previous = SDL_GetPerformanceCounter();
    const double step = 1.0 / 60.0;

    while (running) {
        SDL_Event e;
        // Bounded drain: if the platform ever floods the queue (remote pointer,
        // hotplug churn) the frame still gets to tick and present.
        int eventBudget = 256;
        while (eventBudget-- > 0 && SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                g_windowW = e.window.data1;
                g_windowH = e.window.data2;
            }
            // SDL_RenderSetLogicalSize already maps mouse events into the
            // 960x540 logical space; scaling again by 960/1280 put the cursor
            // (and therefore every capture) up and left of where it was aimed.
            if (e.type == SDL_MOUSEMOTION) {
                setCursor(float(e.motion.x), float(e.motion.y));
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                setCursor(float(e.button.x), float(e.button.y));
                clickAtCursor();
            }
            if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION) {
                setCursor(e.tfinger.x * W, e.tfinger.y * H);
                if (e.type == SDL_FINGERDOWN) clickAtCursor();
            }
            if (e.type == SDL_CONTROLLERDEVICEADDED) openInput(e.cdevice.which);
            if (e.type == SDL_CONTROLLERDEVICEREMOVED) closeInput(e.cdevice.which);
            if (e.type == SDL_JOYDEVICEADDED) openInput(e.jdevice.which);
            if (e.type == SDL_JOYDEVICEREMOVED) closeInput(e.jdevice.which);
            if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                clickAtCursor();
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (e.key.keysym.sym == SDLK_SPACE) clickAtCursor();
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = double(now - previous) / double(SDL_GetPerformanceFrequency());
        previous = now;
        if (dt > 0.25) dt = 0.25;
        accumulator += dt;
        // Never let a slow tick spiral: cap the catch-up so a frame is always
        // drawn (the game runs slow instead of locking up).
        int budget = 4;
        while (accumulator >= step) {
            tick();
            accumulator -= step;
            if (--budget <= 0) { accumulator = 0; break; }
        }
        render();
    }

    g_game.reset();
    for (SDL_GameController* c : g_pads) SDL_GameControllerClose(c);
    g_pads.clear();
    for (SDL_Joystick* j : g_sticks) SDL_JoystickClose(j);
    g_sticks.clear();
    SDL_DestroyTexture(g_sceneTex);
    SDL_DestroyTexture(g_worldTex);
    for (auto& kv : g_textures) if (kv.second) SDL_DestroyTexture(kv.second);
    g_textures.clear();
    audioStopAll();
    if (g_audio.dev) {
        SDL_CloseAudioDevice(g_audio.dev);
        g_audio.dev = 0;
        g_audio.ready = false;
    }
    SDL_DestroyRenderer(g_rend);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

