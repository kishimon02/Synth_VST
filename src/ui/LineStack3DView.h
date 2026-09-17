#pragma once

#include <juce_opengl/juce_opengl.h>
#include "LookAndFeel.h"
#include <atomic>
#include <vector>

namespace ui
{

// Generic OpenGL "stack of polylines" renderer: N lines of P points laid out
// along the depth axis, with optional highlight line, perspective camera,
// mouse rotate / zoom. Subclasses provide the vertex data (on the GL thread,
// when asked) and optionally a 2D fallback / overlay painted with juce.
//
// Cost model: vertex data is rebuilt only when the subclass marks it dirty
// (or every frame if it says so, e.g. a spectrogram); each frame otherwise
// sets a few uniforms and issues one draw per line. Redraws are requested by
// a 30 fps timer while the view is showing - never continuously.
class LineStack3DView : public juce::Component,
                        private juce::OpenGLRenderer,
                        private juce::Timer
{
public:
    explicit LineStack3DView (juce::Colour accent);
    ~LineStack3DView() override;

    void setHighlightLine (int line) noexcept { highlightLine.store (line); }
    void setGLRenderingEnabled (bool enabled) noexcept { glEnabled.store (enabled); }
    bool isGLReady() const noexcept { return glReady.load(); }

    void resetCamera (float rx = 0.55f, float ry = -0.55f, float z = 1.0f);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

protected:
    // Ask for buildVertices() to be called on the next GL frame.
    void requestVertexRefill() noexcept { dirty.store (true); }

    // GL thread. Fill `verts` with numLines * pointsPerLine (x, y, z)
    // triplets, x/y/z roughly in -1..1. Return false to keep the old data.
    virtual bool buildVertices (std::vector<float>& verts, int& numLines, int& pointsPerLine) = 0;
    virtual bool refillEveryFrame() const { return false; }

    // GL thread, every frame: an optional extra line (e.g. the interpolated
    // current frame) drawn last in white. Cheap because it is one line.
    virtual bool buildOverlayLine (std::vector<float>& /*verts*/, int& /*points*/) { return false; }

    // GL thread: colour for a non-highlighted line (t = 0 front .. 1 back).
    virtual juce::Colour lineColour (int line, int numLines) const;
    virtual float lineWidthFor (int /*line*/) const { return 1.0f; }

    // Message thread. Fallback is painted when GL is unavailable or disabled;
    // the overlay is always painted on top.
    virtual void paintFallback (juce::Graphics&) {}
    virtual void paintOverlay (juce::Graphics&) {}
    virtual void timerTick() {}

    juce::Colour accent;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    juce::OpenGLContext context;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> projectionUniform, viewUniform, colourUniform;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> positionAttribute;
    GLuint vbo = 0, vao = 0, overlayVbo = 0;
    std::vector<float> vertexScratch, overlayScratch;
    int numLines = 0, pointsPerLine = 0;

    std::atomic<bool> dirty { true }, glReady { false }, glEnabled { true };
    std::atomic<int> highlightLine { -1 };
    std::atomic<float> rotX { 0.55f }, rotY { -0.55f }, zoom { 1.0f };
    juce::Point<float> dragStart;
    float dragRotX = 0.0f, dragRotY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LineStack3DView)
};

} // namespace ui
