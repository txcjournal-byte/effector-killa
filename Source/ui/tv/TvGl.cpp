#include "TvGl.h"

namespace ek::ui
{
using namespace juce;
using namespace juce::gl;

static const char* vertexShader = R"(
attribute vec2 position;
varying vec2 vUv;
void main()
{
    vUv = position * 0.5 + 0.5;
    gl_Position = vec4 (position, 0.0, 1.0);
}
)";

const char* TvGl::sceneFragmentShader()
{
    return R"(
varying vec2 vUv;
uniform float uTime;
uniform float uBeat;
uniform float uLevel;
uniform float uTrans;
uniform float uCalm;
uniform float uChannel;
uniform vec3 uColour;
uniform float uVillain;
uniform float uAura;
uniform float uDrip;
uniform float uKnock;
uniform float uAspect;

float hash (vec2 p) { return fract (sin (dot (p, vec2 (127.1, 311.7))) * 43758.5453); }
float noise (vec2 p)
{
    vec2 i = floor (p);
    vec2 f = fract (p);
    f = f * f * (3.0 - 2.0 * f);
    return mix (mix (hash (i), hash (i + vec2 (1.0, 0.0)), f.x),
                mix (hash (i + vec2 (0.0, 1.0)), hash (i + vec2 (1.0, 1.0)), f.x), f.y);
}
float fbm (vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) { v += a * noise (p); p *= 2.03; a *= 0.5; }
    return v;
}
float glowLine (float d, float w) { return exp (-abs (d) / w); }

vec3 scene (vec2 uv, float t, float beat)
{
    vec3 c = uColour;
    float lev = uLevel;
    float bf = fract (beat);
    float pulse = pow (1.0 - bf, 3.0) * (1.0 - 0.7 * uCalm);
    vec2 p = vec2 ((uv.x - 0.5) * uAspect, uv.y - 0.5);
    float ch = floor (uChannel + 0.5);
    vec3 col = mix (c * 0.08, c * 0.35, uv.y);

    if (ch < 0.5) // MELODIA - melody lines + stars
    {
        col = mix (vec3 (0.03, 0.0, 0.07), c * 0.25, uv.y);
        for (int k = 0; k < 4; k++)
        {
            float fk = float (k);
            float y0 = 0.28 + 0.14 * fk + 0.06 * (1.0 + 2.0 * lev) * sin (uv.x * (7.0 + 3.0 * fk) + t * (1.0 + 0.4 * fk));
            col += (c + 0.3) * glowLine (uv.y - y0, 0.006) * (0.9 - 0.15 * fk);
        }
        col += vec3 (step (0.997, hash (floor (uv * vec2 (160.0, 90.0)))) * (0.5 + 0.5 * sin (t * 3.0 + uv.x * 40.0)));
    }
    else if (ch < 1.5) // VOX DEI - rings from the voice
    {
        float d = length (p);
        float r = fract (d * 3.5 - t * 0.45);
        col += c * glowLine (r - 0.5, 0.03) * exp (-d * 1.6) * (1.0 + 2.0 * lev);
        col += c * 0.8 * glowLine (d - 0.05 - 0.05 * lev, 0.02);
    }
    else if (ch < 2.5) // SVBTERRA - 808 speaker cone
    {
        float d = length (p) / (1.0 + 0.06 * pulse + 0.25 * lev);
        float rings = 0.5 + 0.5 * cos (d * 70.0);
        col = mix (vec3 (0.02), c * 0.6, rings * smoothstep (0.45, 0.0, d));
        col += c * 2.0 * smoothstep (0.07, 0.0, d);
        float wave = 0.14 + sin (uv.x * 18.0 + t * 7.0) * (0.02 + 0.08 * lev);
        col += (c + 0.4) * glowLine (uv.y - wave, 0.006);
    }
    else if (ch < 3.5) // DRVM CVLT - pads on the beat
    {
        vec2 g = vec2 ((uv.x - 0.14) / 0.72, (uv.y - 0.08) / 0.84) * 4.0;
        vec2 cell = floor (g);
        vec2 f = fract (g);
        float inside = step (0.0, g.x) * step (g.x, 4.0) * step (0.0, g.y) * step (g.y, 4.0);
        float pad = step (0.08, f.x) * step (f.x, 0.92) * step (0.08, f.y) * step (f.y, 0.92) * inside;
        float idx = cell.x + (3.0 - cell.y) * 4.0;
        float stepNow = mod (floor (beat * 4.0), 16.0);
        float lit = 1.0 - step (0.5, abs (idx - stepNow));
        col = mix (vec3 (0.03, 0.02, 0.01), mix (c * 0.18, c * 1.4, lit), pad);
    }
    else if (ch < 4.5) // RAGE ENGINE - strobe
    {
        float strobe = pow (1.0 - bf, 4.0) * (1.0 - 0.8 * uCalm);
        float stripes = step (0.5, fract ((uv.x + uv.y * 0.6) * 6.0 - t * 2.0));
        col = mix (c * 0.12 * (0.4 + 0.6 * stripes), c, strobe * (0.5 + 0.5 * stripes));
        col += c * 0.25 * (1.0 - stripes) * lev;
        col += vec3 (1.0, 0.4, 0.3) * glowLine (uv.y - 0.5 - 0.3 * sin (uv.x * 9.0 + t * 4.0) * (0.3 + lev), 0.01) * 0.6;
    }
    else if (ch < 5.5) // DREAMCORE - clouds
    {
        vec3 sky = mix (vec3 (0.55, 0.65, 1.0), c, uv.y);
        float cl = fbm (vec2 (uv.x * 3.0 * uAspect + t * 0.05, uv.y * 3.0));
        col = mix (sky, vec3 (1.0, 0.97, 0.98), smoothstep (0.45, 0.8, cl));
    }
    else if (ch < 6.5) // VHS - colour bars + tracking noise
    {
        float bar = floor (uv.x * 7.0);
        vec3 pal0 = vec3 (0.75, 0.75, 0.75);
        vec3 pal1 = vec3 (0.75, 0.75, 0.0);
        vec3 pal2 = vec3 (0.0, 0.75, 0.75);
        vec3 pal3 = vec3 (0.0, 0.75, 0.0);
        vec3 pal4 = vec3 (0.75, 0.0, 0.75);
        vec3 pal5 = vec3 (0.75, 0.0, 0.0);
        vec3 pal6 = vec3 (0.0, 0.0, 0.75);
        col = bar < 0.5 ? pal0 : bar < 1.5 ? pal1 : bar < 2.5 ? pal2 : bar < 3.5 ? pal3 : bar < 4.5 ? pal4 : bar < 5.5 ? pal5 : pal6;
        col *= step (0.28, uv.y);
        float band = fract (uv.y - t * 0.12);
        col += vec3 (hash (vec2 (floor (uv.y * 200.0), floor (t * 30.0))) * smoothstep (0.08, 0.0, abs (band - 0.5)) * 0.8);
    }
    else if (ch < 7.5) // MVTANT - toxic metaballs
    {
        float field = 0.0;
        for (int i = 0; i < 6; i++)
        {
            float fi = float (i);
            vec2 b = vec2 (0.35 * uAspect * sin (t * (0.3 + 0.13 * fi) + fi), 0.35 * cos (t * (0.4 + 0.07 * fi) + 2.0 * fi));
            field += (0.012 + 0.01 * lev + 0.004 * pulse) / dot (p - b, p - b);
        }
        col = mix (vec3 (0.01, 0.03, 0.0), c, smoothstep (0.8, 1.6, field));
        col += c * 0.4 * smoothstep (0.3, 0.8, field);
    }
    else if (ch < 8.5) // NO FILTER - raw scope
    {
        float y = 0.5 + (noise (vec2 (uv.x * 30.0, t * 12.0)) - 0.5) * (0.15 + 0.9 * lev);
        col = vec3 (0.0) + vec3 (1.0) * glowLine (uv.y - y, 0.004) + vec3 (0.06) * step (0.98, fract (uv.x * 10.0));
    }
    else if (ch < 9.5) // DRIFT - night highway
    {
        float horizon = 0.54;
        if (uv.y > horizon)
        {
            col = mix (vec3 (0.16, 0.10, 0.16), vec3 (0.01, 0.01, 0.04), (uv.y - horizon) / (1.0 - horizon));
            float bx = floor (uv.x * 60.0);
            float bh = horizon + 0.02 + 0.13 * hash (vec2 (bx, 3.0));
            if (uv.y < bh)
            {
                col = vec3 (0.02, 0.02, 0.035);
                vec2 w = floor (uv * vec2 (240.0, 160.0));
                col += vec3 (1.0, 0.7, 0.3) * step (0.93, hash (w)) * 0.8;
            }
        }
        else
        {
            float z = (horizon - uv.y) / horizon;           // 0 at horizon, 1 at the bottom
            float roadHalf = 0.02 + 0.9 * z;
            float x = (uv.x - 0.5);
            float onRoad = step (abs (x), roadHalf);
            col = mix (vec3 (0.02, 0.015, 0.02), vec3 (0.05, 0.045, 0.05), onRoad);
            float depth = 1.0 / (z + 0.02);
            float dash = step (0.5, fract (depth * 0.9 + t * 3.0));
            float lane = glowLine (abs (x) - roadHalf * 0.33, 0.004 + 0.01 * z) * dash;
            col += vec3 (1.0, 0.75, 0.35) * lane * 1.2;
            col += vec3 (1.0, 0.55, 0.15) * glowLine (abs (x) - roadHalf, 0.004 + 0.01 * z) * 0.9;
            // wet reflections of the lamps
            col += vec3 (1.0, 0.55, 0.2) * 0.35 * z * (0.5 + 0.5 * sin (uv.x * 40.0 + t * 2.0)) * smoothstep (0.6, 0.0, abs (x) - roadHalf * 0.8);
        }
        // street lamps rushing by
        for (int i = 0; i < 5; i++)
        {
            float fi = float (i);
            float z = fract (fi / 5.0 + t * 0.12);
            z = z * z;
            for (int s = 0; s < 2; s++)
            {
                float side = s == 0 ? -1.0 : 1.0;
                vec2 lp = vec2 (0.5 + side * (0.05 + 0.75 * z), horizon + 0.03 + 0.4 * z);
                vec2 d = (uv - lp) * vec2 (uAspect, 1.0);
                col += vec3 (1.0, 0.65, 0.25) * (0.0006 + 0.004 * z) / (dot (d, d) + 0.0004);
            }
        }
        col += c * 0.08;
    }
    else if (ch < 10.5) // HYPER - glitch tunnel
    {
        float r = max (abs (p.x), abs (p.y) * 1.2);
        float stripes = fract (0.12 / (r + 0.02) + t * 0.8);
        vec3 a = c;
        vec3 b = vec3 (1.0, 0.24, 0.78);
        col = mix (a, b, step (0.5, stripes)) * smoothstep (0.0, 0.3, r) * (0.6 + 0.4 * lev);
        col *= 0.6 + 0.4 * step (0.5, fract (uv.y * 90.0 + t * 5.0));
    }
    else // FINAL BOSS - VU / spectrum
    {
        float bx = floor (uv.x * 16.0);
        float fx = fract (uv.x * 16.0);
        float n = noise (vec2 (bx * 3.1, t * 2.5));
        float h = 0.12 + 0.22 * n + 0.6 * lev * (0.55 + 0.45 * n);
        float seg = step (0.2, fract (uv.y * 30.0));
        float bar = step (0.15, fx) * step (fx, 0.85) * step (uv.y, h) * seg;
        vec3 metal = mix (vec3 (0.35), vec3 (0.95), uv.y) * c;
        col = mix (vec3 (0.02), metal, bar);
        col += vec3 (1.0, 0.2, 0.1) * step (0.8, uv.y) * bar;
    }

    // macro colouring
    float villain = clamp ((uVillain - 0.5) * 2.0, -1.0, 1.0);
    col *= villain > 0.0 ? 1.0 - 0.65 * villain : 1.0 - 0.15 * villain;
    col += (c + 0.3) * 0.35 * uTrans * uKnock;
    return col;
}

void main()
{
    float t = uTime * (1.0 - 0.65 * uCalm);
    vec2 uv = vUv;
    float drip = max (0.0, (uDrip - 0.5) * 2.0) * (1.0 - uCalm);
    uv.x += sin (uv.y * 22.0 + t * 3.0) * 0.012 * drip;
    uv.y += sin (uv.x * 14.0 + t * 2.0) * 0.008 * drip;
    gl_FragColor = vec4 (scene (uv, t, uBeat), 1.0);
}
)";
}

const char* TvGl::crtFragmentShader()
{
    return R"(
varying vec2 vUv;
uniform sampler2D uScene;
uniform sampler2D uOsd;
uniform sampler2D uLogo;
uniform float uTime;
uniform float uGlitch;
uniform float uSnow;
uniform float uBlue;
uniform float uPower;
uniform float uPocket;
uniform float uCrash;
uniform float uAura;
uniform float uCalm;
uniform float uHasOsd;
uniform float uHasLogo;
uniform vec2 uRes;
uniform vec2 uDot;
uniform float uDotOn;
uniform vec2 uPath[64];
uniform float uPathCount;
uniform float uPathClosed;
uniform vec2 uLogoPos;
uniform float uLogoOn;
uniform vec2 uLogoSize;
uniform float uCorner;
uniform float uWarp;

float hash (vec2 p) { return fract (sin (dot (p, vec2 (127.1, 311.7))) * 43758.5453); }

vec2 curve (vec2 uv)
{
    uv = uv * 2.0 - 1.0;
    vec2 off = abs (uv.yx) / vec2 (7.0, 5.5);
    uv = uv + uv * off * off;
    return uv * 0.5 + 0.5;
}

float segDist (vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp (dot (pa, ba) / max (dot (ba, ba), 1e-6), 0.0, 1.0);
    return length (pa - ba * h);
}

void main()
{
    // rounded glass mask (in output pixels)
    vec2 px = vUv * uRes;
    vec2 q = abs (px - uRes * 0.5) - (uRes * 0.5 - vec2 (uCorner));
    float cornerDist = length (max (q, 0.0)) - uCorner;
    if (cornerDist > 0.0) { gl_FragColor = vec4 (0.0, 0.0, 0.0, 1.0); return; }

    vec2 uv = curve (vUv);
    vec3 col = vec3 (0.0);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { gl_FragColor = vec4 (0.0, 0.0, 0.0, 1.0); return; }

    // POCKET TV: smaller, softer picture
    float shrink = 1.0 - 0.38 * uPocket;
    vec2 puv = (uv - 0.5) / shrink + 0.5;
    // OFF AIR: squash to a line, then to a dot
    float sy = uPower > 0.4 ? mix (0.012, 1.0, (uPower - 0.4) / 0.6) : 0.012;
    float sx = uPower > 0.4 ? 1.0 : mix (0.01, 1.0, uPower / 0.4);
    puv = (puv - 0.5) / vec2 (sx, sy) + 0.5;
    bool inPic = puv.x >= 0.0 && puv.x <= 1.0 && puv.y >= 0.0 && puv.y <= 1.0 && uPower > 0.001;

    if (inPic)
    {
        // tearing / glitch
        float g = max (uGlitch, max (0.0, uCrash - 0.6) * 0.8 * (1.0 - uCalm));
        float row = floor (puv.y * 48.0);
        float tt = floor (uTime * 24.0);
        if (hash (vec2 (row, tt)) < g * 0.55) puv.x += (hash (vec2 (row + 7.0, tt)) - 0.5) * 0.25 * g;
        puv.x += sin (puv.y * 300.0 + uTime * 50.0) * 0.0015 * g;

        // AV1 REMOTE: the picture bends around the dot
        if (uWarp > 0.5)
        {
            vec2 m0 = vec2 (30.0) / uRes;
            vec2 dp = m0 + uDot * (1.0 - 2.0 * m0);
            vec2 dv = (puv - dp) * vec2 (uRes.x / uRes.y, 1.0);
            float dl = length (dv);
            puv -= (puv - dp) * 0.35 * exp (-dl * 7.0);
        }

        float ca = 0.0015 + 0.004 * g + 0.002 * uPocket;
        col.r = texture2D (uScene, puv + vec2 (ca, 0.0)).r;
        col.g = texture2D (uScene, puv).g;
        col.b = texture2D (uScene, puv - vec2 (ca, 0.0)).b;

        // AURA: glow / soft focus
        float glow = max (0.0, (uAura - 0.5) * 2.0) + 0.6 * uPocket;
        if (glow > 0.01)
        {
            vec3 blur = vec3 (0.0);
            for (int i = 0; i < 8; i++)
            {
                float a = float (i) * 0.785398;
                blur += texture2D (uScene, puv + vec2 (cos (a), sin (a)) * 0.012).rgb;
            }
            col = mix (col, blur / 8.0, 0.5 * glow) + blur / 8.0 * 0.35 * glow;
        }

        // play layer (coordinates: 0..1 with 30 px margins, like the software renderer)
        vec2 m = vec2 (30.0) / uRes;
        vec2 pn = (puv - m) / (1.0 - 2.0 * m);
        vec2 aspect = vec2 (uRes.x / uRes.y, 1.0);
        int n = int (uPathCount);
        if (n > 1)
        {
            float d = 10.0;
            for (int i = 0; i < 63; i++)
            {
                if (i + 1 >= n) break;
                d = min (d, segDist (pn * aspect, uPath[i] * aspect, uPath[i + 1] * aspect));
            }
            if (uPathClosed > 0.5) d = min (d, segDist (pn * aspect, uPath[n - 1] * aspect, uPath[0] * aspect));
            col += vec3 (1.0, 0.85, 0.6) * exp (-d * 420.0) * 1.3 + vec3 (1.0, 0.6, 0.2) * exp (-d * 70.0) * 0.35;
        }
        if (uDotOn > 0.5)
        {
            float dd = length ((pn - uDot) * aspect);
            col += vec3 (1.0, 0.95, 0.85) * (exp (-dd * 160.0) * 2.0 + exp (-dd * 25.0) * 0.5);
        }
        if (uLogoOn > 0.5 && uHasLogo > 0.5)
        {
            vec2 lc = uLogoSize * 0.5 + uLogoPos * (1.0 - uLogoSize);
            vec2 luv = (puv - (lc - uLogoSize * 0.5)) / uLogoSize;
            if (luv.x >= 0.0 && luv.x <= 1.0 && luv.y >= 0.0 && luv.y <= 1.0)
            {
                vec4 lg = texture2D (uLogo, luv);
                col = mix (col, lg.rgb, lg.a);
            }
        }

        // OSD (drawn by JUCE, premultiplied)
        if (uHasOsd > 0.5)
        {
            vec4 osd = texture2D (uOsd, puv);
            col = col * (1.0 - osd.a) + osd.rgb;
        }

        // channel switch snow / NO SIGNAL blue
        float sn = hash (floor (puv * vec2 (320.0, 180.0)) + fract (uTime * 13.0) * 100.0);
        col = mix (col, vec3 (sn), uSnow);
        if (uBlue > 0.5)
        {
            vec4 osd = texture2D (uOsd, puv);
            col = vec3 (0.06, 0.18, 0.8) * (1.0 - osd.a) + osd.rgb;
        }
        // brighten towards white while switching off
        col = mix (col, vec3 (1.0), clamp ((1.0 - uPower) * 1.4, 0.0, 1.0));
    }

    // CRT: scanlines, vignette, grain
    float scan = 0.82 + 0.18 * sin (uv.y * uRes.y * 1.0472);
    col *= scan;
    vec2 v = uv * (1.0 - uv.yx);
    col *= pow (clamp (v.x * v.y * 18.0, 0.0, 1.0), 0.28);
    col += (hash (px + fract (uTime) * 91.0) - 0.5) * 0.035;
    // glass reflection
    col += vec3 (0.05) * smoothstep (0.6, 0.0, length (vUv - vec2 (0.2, 0.85)));
    gl_FragColor = vec4 (col, 1.0);
}
)";
}

// ============================================================================
TvGl::TvGl (Component& t, std::function<Rectangle<int>()> b) : target (t), tvBounds (std::move (b))
{
    context.setRenderer (this);
    // Only the TV is drawn by OpenGL (the rest of the UI keeps the software renderer). JUCE's GL
    // component painting needs shaders and would crash on GL 1.1 machines (VMs, remote desktop).
    context.setComponentPaintingEnabled (false);
    context.setContinuousRepainting (true);
    context.setOpenGLVersionRequired (OpenGLContext::openGL3_2);
    context.setMultisamplingEnabled (false);
}

TvGl::~TvGl() { detach(); }

void TvGl::attach() { context.attachTo (target); }
void TvGl::detach()
{
    if (context.isAttached()) context.detach();
    working = false;
}

void TvGl::setFrame (const TvFrame& f)
{
    const auto vb = tvBounds();
    const SpinLock::ScopedLockType sl (lock);
    frame = f;
    viewport = vb;
}

void TvGl::setOsd (const Image& img)
{
    const SpinLock::ScopedLockType sl (lock);
    pendingOsd = img;
}

void TvGl::setLogo (const Image& img)
{
    const SpinLock::ScopedLockType sl (lock);
    pendingLogo = img;
}

bool TvGl::compile (std::unique_ptr<OpenGLShaderProgram>& prog, const char* fragment)
{
    prog = std::make_unique<OpenGLShaderProgram> (context);
    // translate to GLSL 1.50 when the context is 3.2+ (no-op otherwise)
    const String vs = OpenGLHelpers::translateVertexShaderToV3 (vertexShader);
    const String fs = OpenGLHelpers::translateFragmentShaderToV3 (fragment);
    if (! prog->addVertexShader (vs) || ! prog->addFragmentShader (fs) || ! prog->link())
    {
        std::fprintf (stderr, "Effector Killa: TV shader error (falling back to software): %s\n", prog->getLastError().toRawUTF8());
        prog.reset();
        return false;
    }
    return true;
}

void TvGl::newOpenGLContextCreated()
{
    const char* version = (const char*) glGetString (GL_VERSION);
    if (std::getenv ("EK_GL_DEBUG") != nullptr)
        std::fprintf (stderr, "Effector Killa: OpenGL %s\n", version != nullptr ? version : "?");
    // needs GLSL shaders + buffer objects (GL 2.1+); otherwise fall back to the software TV
    const int major = version != nullptr ? String (version).getIntValue() : 0;
    if (major < 2 || OpenGLShaderProgram::getLanguageVersion() <= 0) { failed = true; return; }
    const bool ok = compile (scene, sceneFragmentShader()) && compile (crt, crtFragmentShader());
    if (! ok) { failed = true; return; }
    const float quad[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (quad), quad, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    hasOsd = hasLogo = false;
}

void TvGl::openGLContextClosing()
{
    scene.reset();
    crt.reset();
    sceneFbo.release();
    osdTex.release();
    logoTex.release();
    if (vbo != 0) { glDeleteBuffers (1, &vbo); vbo = 0; }
    working = false;
}

void TvGl::drawQuad (OpenGLShaderProgram& prog)
{
    const GLint pos = glGetAttribLocation (prog.getProgramID(), "position");
    if (pos < 0) return;
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray ((GLuint) pos);
    glVertexAttribPointer ((GLuint) pos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray ((GLuint) pos);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void TvGl::renderOpenGL()
{
    OpenGLHelpers::clear (Colours::black);
    if (scene == nullptr || crt == nullptr) return;

    TvFrame f;
    Rectangle<int> vb;
    Image osd, logo;
    {
        const SpinLock::ScopedLockType sl (lock);
        f = frame;
        vb = viewport;
        osd = pendingOsd; pendingOsd = {};
        logo = pendingLogo; pendingLogo = {};
    }
    if (osd.isValid()) { osdTex.loadImage (osd); hasOsd = true; }
    if (logo.isValid()) { logoTex.loadImage (logo); hasLogo = true; }
    if (vb.isEmpty()) return;

    const double scale = context.getRenderingScale();
    const int fbH = roundToInt (scale * target.getHeight());
    const int x = roundToInt (vb.getX() * scale), w = roundToInt (vb.getWidth() * scale);
    const int h = roundToInt (vb.getHeight() * scale);
    const int y = fbH - roundToInt (vb.getBottom() * scale);

    // ---- pass 1: scene at half resolution ------------------------------------------------
    const int sw = jmax (160, w / 2), sh = jmax (90, h / 2);
    if (sceneFbo.getWidth() != sw || sceneFbo.getHeight() != sh)
    {
        sceneFbo.release();
        sceneFbo.initialise (context, sw, sh);
    }
    sceneFbo.makeCurrentRenderingTarget();
    glViewport (0, 0, sw, sh);
    glDisable (GL_BLEND);
    scene->use();
    scene->setUniform ("uTime", f.time);
    scene->setUniform ("uBeat", f.beat);
    scene->setUniform ("uLevel", f.level);
    scene->setUniform ("uTrans", f.transient);
    scene->setUniform ("uCalm", f.calm);
    scene->setUniform ("uChannel", (float) f.channel);
    scene->setUniform ("uColour", f.colour.getFloatRed(), f.colour.getFloatGreen(), f.colour.getFloatBlue());
    scene->setUniform ("uVillain", f.macros[0]);
    scene->setUniform ("uAura", f.macros[2]);
    scene->setUniform ("uDrip", f.macros[3]);
    scene->setUniform ("uKnock", f.macros[4]);
    scene->setUniform ("uAspect", (float) w / (float) jmax (1, h));
    drawQuad (*scene);
    sceneFbo.releaseAsRenderingTarget();

    // ---- pass 2: CRT composite into the TV viewport ------------------------------------------
    glViewport (x, y, w, h);
    crt->use();
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, sceneFbo.getTextureID());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    crt->setUniform ("uScene", 0);
    glActiveTexture (GL_TEXTURE1);
    if (hasOsd) osdTex.bind(); else glBindTexture (GL_TEXTURE_2D, 0);
    crt->setUniform ("uOsd", 1);
    glActiveTexture (GL_TEXTURE2);
    if (hasLogo) logoTex.bind(); else glBindTexture (GL_TEXTURE_2D, 0);
    crt->setUniform ("uLogo", 2);
    glActiveTexture (GL_TEXTURE0);

    crt->setUniform ("uTime", f.time);
    crt->setUniform ("uGlitch", f.glitch);
    crt->setUniform ("uSnow", f.snow);
    crt->setUniform ("uBlue", f.blue);
    crt->setUniform ("uPower", f.power);
    crt->setUniform ("uPocket", f.pocket);
    crt->setUniform ("uCrash", f.macros[1]);
    crt->setUniform ("uAura", f.macros[2]);
    crt->setUniform ("uCalm", f.calm);
    crt->setUniform ("uHasOsd", hasOsd ? 1.0f : 0.0f);
    crt->setUniform ("uHasLogo", hasLogo ? 1.0f : 0.0f);
    crt->setUniform ("uRes", (float) w, (float) h);
    crt->setUniform ("uDot", f.dot.x, f.dot.y);
    crt->setUniform ("uDotOn", f.dotVisible ? 1.0f : 0.0f);
    GLfloat pts[128] {};
    const int n = jmin (64, (int) f.path.size());
    for (int i = 0; i < n; ++i) { pts[i * 2] = f.path[(size_t) i].x; pts[i * 2 + 1] = f.path[(size_t) i].y; }
    const GLint pathLoc = glGetUniformLocation (crt->getProgramID(), "uPath");
    if (pathLoc >= 0) glUniform2fv (pathLoc, 64, pts);
    crt->setUniform ("uPathCount", (float) n);
    crt->setUniform ("uPathClosed", f.avMode == 1 && n > 2 && f.dotVisible ? 1.0f : 0.0f);
    crt->setUniform ("uLogoPos", f.logo.x, f.logo.y);
    crt->setUniform ("uLogoOn", f.logoVisible ? 1.0f : 0.0f);
    crt->setUniform ("uLogoSize", 0.3f, 0.3f * 0.59f * (float) w / (float) jmax (1, h));
    crt->setUniform ("uWarp", f.avMode == 0 ? 1.0f : 0.0f);
    crt->setUniform ("uCorner", (float) (34.0 * (double) h / 471.0));
    drawQuad (*crt);

    glBindTexture (GL_TEXTURE_2D, 0);
    working = true;
}

} // namespace ek::ui
