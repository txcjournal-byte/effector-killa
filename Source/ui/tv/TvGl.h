#pragma once

// OpenGL renderer of the TV picture. Attached to the editor (top-level component); JUCE paints the
// rest of the UI on top, the TV area is left transparent by the cabinet.
//  pass 1: procedural channel scene (12 scenes, GLSL) into a half-resolution frame buffer
//  pass 2: CRT – curvature, tearing, chroma, glow, play layer (dot / path / logo), OSD, scanlines

#include <juce_opengl/juce_opengl.h>
#include "TvState.h"

namespace ek::ui
{
class TvGl : public juce::OpenGLRenderer
{
public:
    TvGl (juce::Component& target, std::function<juce::Rectangle<int>()> tvBoundsInTarget);
    ~TvGl() override;

    void attach();
    void detach();
    bool isWorking() const noexcept { return working.load(); }
    bool hasFailed() const noexcept { return failed.load(); }

    // message thread
    void setFrame (const TvFrame& f);
    void setOsd (const juce::Image& img);
    void setLogo (const juce::Image& img);

    // OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    static const char* sceneFragmentShader();
    static const char* crtFragmentShader();

private:
    bool compile (std::unique_ptr<juce::OpenGLShaderProgram>& prog, const char* fragment);
    void drawQuad (juce::OpenGLShaderProgram& prog);

    juce::Component& target;
    std::function<juce::Rectangle<int>()> tvBounds;
    juce::OpenGLContext context;

    std::unique_ptr<juce::OpenGLShaderProgram> scene, crt;
    juce::OpenGLFrameBuffer sceneFbo;
    juce::OpenGLTexture osdTex, logoTex;
    GLuint vbo = 0;

    juce::SpinLock lock;
    TvFrame frame;
    juce::Image pendingOsd, pendingLogo;
    juce::Rectangle<int> viewport;
    bool hasOsd = false, hasLogo = false;
    std::atomic<bool> working { false }, failed { false };
};
} // namespace ek::ui
