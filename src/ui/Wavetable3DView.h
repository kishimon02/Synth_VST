#pragma once

#include <juce_opengl/juce_opengl.h>
#include "../dsp/Wavetable.h"
#include "LookAndFeel.h"

namespace ui
{

// Serum-style 3D wavetable view: every frame is one polyline, stacked along
// the depth axis, the frame under the WT position highlighted.
//
// Cost: vertex data is uploaded once per table change (frames x 256 points);
// each redraw only sets a few uniforms and issues one line-strip draw per
// frame. Redraw is triggered from a 30 fps timer while the view is showing,
// never continuously. If the GL context cannot be created, paint() draws a
// 2D stacked fallback instead.
class Wavetable3DView final : public juce::Component,
                              private juce::OpenGLRenderer,
                              private juce::Timer
{
public:
    explicit Wavetable3DView (juce::Colour accent = colours::accent);
    ~Wavetable3DView() override;

    // Message thread. The table must outlive the view (bank is append-only).
    void setTable (const wf::Wavetable* table);
    void setPosition (float pos01);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int pointsPerFrame = 256;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    void uploadTable (const wf::Wavetable* table);
    void paintFallback (juce::Graphics&);

    juce::OpenGLContext context;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> projectionUniform, viewUniform, colourUniform;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> positionAttribute;
    GLuint vbo = 0, vao = 0;
    int uploadedFrames = 0;

    std::atomic<const wf::Wavetable*> pendingTable { nullptr };
    const wf::Wavetable* uploadedTable = nullptr;
    const wf::Wavetable* displayedTable = nullptr;   // message thread copy for the label / fallback

    std::atomic<float> position { 0.0f };
    std::atomic<float> rotX { 0.55f }, rotY { -0.55f }, zoom { 1.0f };
    std::atomic<bool> glReady { false };
    juce::Point<float> dragStart;
    float dragRotX = 0.0f, dragRotY = 0.0f;
    juce::Colour accent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Wavetable3DView)
};

} // namespace ui
