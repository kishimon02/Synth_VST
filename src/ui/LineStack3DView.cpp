#include "LineStack3DView.h"

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
            depthFade = clamp ((v.z + 6.0) / 4.0, 0.15, 1.0);
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

LineStack3DView::LineStack3DView (juce::Colour accentIn) : accent (accentIn)
{
    setOpaque (true);
    juce::OpenGLPixelFormat pf;
    pf.multisamplingLevel = 4;              // smoother lines; ignored if unsupported
    context.setPixelFormat (pf);
    context.setMultisamplingEnabled (true);
    context.setRenderer (this);
    context.setComponentPaintingEnabled (true);   // paint() is composited over the GL frame
    context.setContinuousRepainting (false);
    context.attachTo (*this);
    startTimerHz (30);
}

LineStack3DView::~LineStack3DView()
{
    stopTimer();
    context.detach();
}

void LineStack3DView::resetCamera (float rx, float ry, float z)
{
    rotX.store (rx); rotY.store (ry); zoom.store (z);
}

juce::Colour LineStack3DView::lineColour (int line, int lines) const
{
    const float t = lines > 1 ? (float) line / (float) (lines - 1) : 0.0f;
    const auto dim = accent.withAlpha (lines > 64 ? 0.35f : 0.6f);
    return dim.interpolatedWith (juce::Colours::white.withAlpha (dim.getAlpha()), t * 0.25f);
}

//==============================================================================
void LineStack3DView::timerCallback()
{
    if (! isShowing())
        return;
    timerTick();
    if (glReady.load())
        context.triggerRepaint();
    else
        repaint();
}

void LineStack3DView::newOpenGLContextCreated()
{
    shader = std::make_unique<juce::OpenGLShaderProgram> (context);
    const bool ok = shader->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShaderSource))
                 && shader->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentShaderSource))
                 && shader->link();
    if (! ok)
    {
        DBG ("LineStack3DView shader: " << shader->getLastError());
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
    numLines = pointsPerLine = 0;
    dirty.store (true);
    glReady.store (true);
}

void LineStack3DView::openGLContextClosing()
{
    glReady.store (false);
    positionAttribute.reset();
    projectionUniform.reset(); viewUniform.reset(); colourUniform.reset();
    shader.reset();
    if (vbo != 0) { glDeleteBuffers (1, &vbo); vbo = 0; }
    if (vao != 0 && glDeleteVertexArrays != nullptr) { glDeleteVertexArrays (1, &vao); vao = 0; }
}

void LineStack3DView::renderOpenGL()
{
    const float scale = (float) context.getRenderingScale();
    const int w = juce::roundToInt (scale * (float) getWidth());
    const int h = juce::roundToInt (scale * (float) getHeight());
    juce::OpenGLHelpers::clear (colours::widget);
    if (shader == nullptr || w <= 0 || h <= 0 || ! glEnabled.load())
        return;

    if (dirty.exchange (false) || refillEveryFrame())
    {
        int lines = 0, points = 0;
        if (buildVertices (vertexScratch, lines, points) && lines > 0 && points > 1)
        {
            numLines = lines;
            pointsPerLine = points;
            glBindBuffer (GL_ARRAY_BUFFER, vbo);
            glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (vertexScratch.size() * sizeof (float)),
                          vertexScratch.data(), GL_DYNAMIC_DRAW);
        }
    }
    if (numLines == 0)
        return;

    glViewport (0, 0, w, h);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_DEPTH_TEST);
    glEnable (GL_LINE_SMOOTH);

    shader->use();

    const float aspect = (float) w / (float) h;
    const float fh = 0.9f;
    const auto projection = juce::Matrix3D<float>::fromFrustum (-fh * aspect, fh * aspect, -fh, fh, 2.0f, 40.0f);
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

    const int highlight = highlightLine.load();
    for (int line = numLines - 1; line >= 0; --line)   // back to front
    {
        if (line == highlight)
            continue;
        const auto c = lineColour (line, numLines);
        colourUniform->set (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha());
        glLineWidth (lineWidthFor (line));
        glDrawArrays (GL_LINE_STRIP, line * pointsPerLine, pointsPerLine);
    }

    if (juce::isPositiveAndBelow (highlight, numLines))
    {
        glLineWidth (2.5f);
        colourUniform->set (1.0f, 1.0f, 1.0f, 1.0f);
        glDrawArrays (GL_LINE_STRIP, highlight * pointsPerLine, pointsPerLine);
    }
    glLineWidth (1.0f);

    glDisableVertexAttribArray ((GLuint) positionAttribute->attributeID);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

//==============================================================================
void LineStack3DView::paint (juce::Graphics& g)
{
    if (! glReady.load() || ! glEnabled.load())
    {
        g.fillAll (colours::widget);
        paintFallback (g);
    }
    paintOverlay (g);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void LineStack3DView::mouseDown (const juce::MouseEvent& e)
{
    dragStart = e.position;
    dragRotX = rotX.load();
    dragRotY = rotY.load();
}

void LineStack3DView::mouseDrag (const juce::MouseEvent& e)
{
    if (! glEnabled.load())
        return;
    const auto d = e.position - dragStart;
    rotX.store (juce::jlimit (-1.4f, 1.4f, dragRotX + d.y * 0.01f));
    rotY.store (dragRotY + d.x * 0.01f);
}

void LineStack3DView::mouseDoubleClick (const juce::MouseEvent&)
{
    resetCamera();
}

void LineStack3DView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    zoom.store (juce::jlimit (0.4f, 3.0f, zoom.load() * (1.0f + wheel.deltaY * 0.5f)));
}

} // namespace ui
