#include "Wavetable3DView.h"

using namespace juce::gl;

namespace ui
{

namespace
{
    const char* vertexShaderSource = R"(
        attribute vec3 position;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        varying float depthFade;
        void main()
        {
            vec4 v = viewMatrix * vec4 (position, 1.0);
            depthFade = clamp ((v.z + 7.5) / 4.5, 0.15, 1.0);
            gl_Position = projectionMatrix * v;
        }
    )";

    const char* fragmentShaderSource = R"(
        uniform vec4 colour;
        varying float depthFade;
        void main()
        {
            gl_FragColor = vec4 (colour.rgb, colour.a * depthFade);
        }
    )";
}

Wavetable3DView::Wavetable3DView (juce::Colour accentIn) : accent (accentIn)
{
    setOpaque (true);
    context.setRenderer (this);
    context.setComponentPaintingEnabled (true);   // paint() is drawn over the GL frame
    context.setContinuousRepainting (false);
    context.attachTo (*this);
    startTimerHz (30);
}

Wavetable3DView::~Wavetable3DView()
{
    stopTimer();
    context.detach();
}

void Wavetable3DView::setTable (const wf::Wavetable* table)
{
    if (table == displayedTable)
        return;
    displayedTable = table;
    pendingTable.store (table, std::memory_order_release);
    repaint();
}

void Wavetable3DView::setPosition (float pos01)
{
    position.store (pos01, std::memory_order_relaxed);
}

//==============================================================================
void Wavetable3DView::timerCallback()
{
    if (! isShowing())
        return;
    if (glReady.load())
        context.triggerRepaint();
    else
        repaint();
}

void Wavetable3DView::newOpenGLContextCreated()
{
    shader = std::make_unique<juce::OpenGLShaderProgram> (context);
    const bool ok = shader->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShaderSource))
                 && shader->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentShaderSource))
                 && shader->link();
    if (! ok)
    {
        DBG ("Wavetable3DView shader: " << shader->getLastError());
        shader.reset();
        return;
    }

    projectionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*shader, "projectionMatrix");
    viewUniform       = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*shader, "viewMatrix");
    colourUniform     = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*shader, "colour");
    positionAttribute = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*shader, "position");

    if (glGenVertexArrays != nullptr)
    {
        glGenVertexArrays (1, &vao);
        glBindVertexArray (vao);
    }
    glGenBuffers (1, &vbo);
    uploadedTable = nullptr;
    uploadedFrames = 0;
    glReady.store (true);
}

void Wavetable3DView::openGLContextClosing()
{
    glReady.store (false);
    positionAttribute.reset();
    projectionUniform.reset(); viewUniform.reset(); colourUniform.reset();
    shader.reset();
    if (vbo != 0) { glDeleteBuffers (1, &vbo); vbo = 0; }
    if (vao != 0 && glDeleteVertexArrays != nullptr) { glDeleteVertexArrays (1, &vao); vao = 0; }
    uploadedTable = nullptr;
}

void Wavetable3DView::uploadTable (const wf::Wavetable* table)
{
    uploadedTable = table;
    uploadedFrames = table != nullptr ? table->getNumFrames() : 0;
    if (uploadedFrames == 0)
        return;

    // x: sample position, y: amplitude, z: frame depth (front = frame 0)
    std::vector<float> verts ((size_t) uploadedFrames * pointsPerFrame * 3);
    const int step = wf::Wavetable::frameSize / pointsPerFrame;
    size_t k = 0;
    for (int f = 0; f < uploadedFrames; ++f)
    {
        const float* raw = table->getRawFrame (f);
        const float z = uploadedFrames > 1 ? -1.0f + 2.0f * (float) f / (float) (uploadedFrames - 1) : 0.0f;
        for (int i = 0; i < pointsPerFrame; ++i)
        {
            verts[k++] = -1.0f + 2.0f * (float) i / (float) (pointsPerFrame - 1);
            verts[k++] = juce::jlimit (-1.0f, 1.0f, raw[i * step]) * 0.4f;
            verts[k++] = z;
        }
    }
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (verts.size() * sizeof (float)), verts.data(), GL_STATIC_DRAW);
}

void Wavetable3DView::renderOpenGL()
{
    const float scale = (float) context.getRenderingScale();
    const int w = juce::roundToInt (scale * (float) getWidth());
    const int h = juce::roundToInt (scale * (float) getHeight());
    juce::OpenGLHelpers::clear (colours::widget);
    if (shader == nullptr || w <= 0 || h <= 0)
        return;

    const auto* table = pendingTable.load (std::memory_order_acquire);
    if (table != uploadedTable)
        uploadTable (table);
    if (uploadedFrames == 0)
        return;

    glViewport (0, 0, w, h);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_DEPTH_TEST);

    shader->use();

    const float aspect = (float) w / (float) h;
    const float near = 2.0f, fh = 0.9f;
    const auto projection = juce::Matrix3D<float>::fromFrustum (-fh * aspect, fh * aspect, -fh, fh, near, 40.0f);
    const float dist = 3.4f / juce::jlimit (0.4f, 3.0f, zoom.load());
    // column-major: (T * R) v rotates first, then pushes the scene away from the camera
    const auto view = juce::Matrix3D<float>::fromTranslation ({ 0.0f, 0.0f, -dist })
                    * juce::Matrix3D<float>::rotation ({ rotX.load(), rotY.load(), 0.0f });
    projectionUniform->setMatrix4 (projection.mat, 1, false);
    viewUniform->setMatrix4 (view.mat, 1, false);

    if (vao != 0) glBindVertexArray (vao);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer ((GLuint) positionAttribute->attributeID, 3, GL_FLOAT, GL_FALSE, 3 * sizeof (float), nullptr);
    glEnableVertexAttribArray ((GLuint) positionAttribute->attributeID);

    const float pos = position.load();
    const int highlight = juce::roundToInt (pos * (float) (uploadedFrames - 1));
    const auto dim = accent.withAlpha (uploadedFrames > 64 ? 0.35f : 0.6f);

    glLineWidth (1.0f);
    for (int f = uploadedFrames - 1; f >= 0; --f)   // back to front
    {
        if (f == highlight)
            continue;
        const float t = uploadedFrames > 1 ? (float) f / (float) (uploadedFrames - 1) : 0.0f;
        const auto c = dim.interpolatedWith (juce::Colours::white.withAlpha (dim.getAlpha()), t * 0.25f);
        colourUniform->set (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha());
        glDrawArrays (GL_LINE_STRIP, f * pointsPerFrame, pointsPerFrame);
    }

    glLineWidth (2.5f);
    colourUniform->set (1.0f, 1.0f, 1.0f, 1.0f);
    glDrawArrays (GL_LINE_STRIP, highlight * pointsPerFrame, pointsPerFrame);
    glLineWidth (1.0f);

    glDisableVertexAttribArray ((GLuint) positionAttribute->attributeID);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

//==============================================================================
void Wavetable3DView::paint (juce::Graphics& g)
{
    if (! glReady.load())
        paintFallback (g);

    if (displayedTable != nullptr)
    {
        const int frames = displayedTable->getNumFrames();
        const int current = juce::roundToInt (position.load() * (float) (frames - 1)) + 1;
        g.setColour (colours::text.withAlpha (0.75f));
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (displayedTable->getName() + "   " + juce::String (current) + " / " + juce::String (frames),
                    getLocalBounds().reduced (8, 5), juce::Justification::topLeft);
    }
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void Wavetable3DView::paintFallback (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (colours::widget);
    if (displayedTable == nullptr)
        return;

    // Stacked 2D frames, offset diagonally, current one on top in white.
    const int frames = displayedTable->getNumFrames();
    const int shown = juce::jmin (frames, 24);
    const int highlight = juce::roundToInt (position.load() * (float) (frames - 1));
    const float dx = r.getWidth() * 0.25f / (float) juce::jmax (1, shown);
    const float dy = r.getHeight() * 0.5f / (float) juce::jmax (1, shown);
    const float lineW = r.getWidth() * 0.7f, amp = r.getHeight() * 0.18f;

    auto drawFrame = [&] (int f, juce::Colour c, float thickness)
    {
        const int slot = frames > 1 ? (int) ((long long) f * (shown - 1) / (frames - 1)) : 0;
        const float x0 = r.getX() + 10.0f + dx * (float) slot;
        const float y0 = r.getBottom() - 16.0f - dy * (float) slot;
        const float* raw = displayedTable->getRawFrame (f);
        juce::Path p;
        for (int i = 0; i < pointsPerFrame; ++i)
        {
            const float x = x0 + lineW * (float) i / (float) (pointsPerFrame - 1);
            const float y = y0 - raw[i * (wf::Wavetable::frameSize / pointsPerFrame)] * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness));
    };

    for (int s = shown - 1; s >= 0; --s)
    {
        const int f = frames > 1 ? (int) ((long long) s * (frames - 1) / juce::jmax (1, shown - 1)) : 0;
        drawFrame (f, accent.withAlpha (0.35f), 1.0f);
    }
    drawFrame (highlight, juce::Colours::white, 2.0f);
}

//==============================================================================
void Wavetable3DView::mouseDown (const juce::MouseEvent& e)
{
    dragStart = e.position;
    dragRotX = rotX.load();
    dragRotY = rotY.load();
}

void Wavetable3DView::mouseDrag (const juce::MouseEvent& e)
{
    const auto d = e.position - dragStart;
    rotX.store (juce::jlimit (-1.4f, 1.4f, dragRotX + d.y * 0.01f));
    rotY.store (dragRotY + d.x * 0.01f);
}

void Wavetable3DView::mouseDoubleClick (const juce::MouseEvent&)
{
    rotX.store (0.55f); rotY.store (-0.55f); zoom.store (1.0f);
}

void Wavetable3DView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    zoom.store (juce::jlimit (0.4f, 3.0f, zoom.load() * (1.0f + wheel.deltaY * 0.5f)));
}

} // namespace ui
