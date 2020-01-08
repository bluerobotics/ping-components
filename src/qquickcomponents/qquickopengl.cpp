#include "qquickopengl.h"

#include <QQuickWindow>

#include <math.h>

#include <QDebug>
#include <QTimer>

/*
struct MaterialState
{
    QSGTexture *source;
};

class MaterialShader : public QSGSimpleMaterialShader<MaterialState>
{
    QSG_DECLARE_SIMPLE_SHADER(MaterialShader, MaterialState)

    MaterialShader()
    {
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/opengl/qquickopengl/fragment.glsl"));
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/opengl/qquickopengl/vertex.glsl"));
    }

    QList<QByteArray> attributes() const
    {
        return QList<QByteArray>() << "qt_Vertex" << "qt_MultiTexCoord0";
    }

    void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
    {
        Q_ASSERT(program()->isLinked());
        if (state.isOpacityDirty()) {
            program()->setUniformValue(m_id_opacity, state.opacity());
        }
        if (state.isMatrixDirty()) {
            program()->setUniformValue(m_id_matrix, state.combinedMatrix());
        }
    }
}*/

class Material : public QSGMaterial
{
public:
    QSGMaterialType *type() const { static QSGMaterialType type; return &type; }
    QSGMaterialShader *createShader() const { return new Shader; }
    int toto() { return i++; }
private:
    int i;
};

char const *const *Shader::attributeNames() const
{
    static char const *const names[] = {"qt_Vertex", "qt_MultiTexCoord0", 0 };
    return names;
}

void Shader::loadDefaultShader()
{
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/opengl/qquickopengl/fragment.glsl"));
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/opengl/qquickopengl/vertex.glsl"));
}

void Shader::initialize()
{
    QSGMaterialShader::initialize();
    _matrixId = program()->uniformLocation("qt_Matrix");
    _opacityId = program()->uniformLocation("qt_Opacity");
}

void Shader::updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    Q_ASSERT(program()->isLinked());
    if (state.isMatrixDirty()) {
        program()->setUniformValue(_matrixId, state.combinedMatrix());
    }
    if (state.isOpacityDirty()) {
        program()->setUniformValue(_opacityId, state.opacity());
    }

    // Let the child class to receive and do the necessary updates with the new state
    update(state, newMaterial, oldMaterial);
}

void Shader::update(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    Q_UNUSED(state)
    Q_UNUSED(newMaterial)
    Q_UNUSED(oldMaterial)
}

QQuickOpenGL::QQuickOpenGL(QQuickItem *parent)
    : QQuickItem(parent)
    , _shaderProgram(nullptr)
    , _shift(0)
    , _window(nullptr)
{
    // We are going to fill our own content with OpenGL
    QQuickItem::setFlag(QQuickItem::ItemHasContents);

    connect(this, &QQuickItem::windowChanged, this, &QQuickOpenGL::handleWindowChanged);
    /*
    connect(this, &QQuickOpenGL::shiftChanged, this, [this]{
        if (_window) {
            _window->update();
        }
    });

    auto timer = new QTimer();
    connect(timer, &QTimer::timeout, this, &QQuickOpenGL::shiftChanged);
    timer->start(1000/60);
    */
}

void QQuickOpenGL::handleWindowChanged(QQuickWindow* window)
{
    if (!window) {
        return;
    }

    _window = window;
    window->setClearBeforeRendering(false);
}


QSGNode *QQuickOpenGL::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    QSGSimpleRectNode *simpleRectNode = static_cast<QSGSimpleRectNode *>(node);
    if (!simpleRectNode) {
        simpleRectNode = new QSGSimpleRectNode();
        simpleRectNode->setMaterial(new Material());
        simpleRectNode->material()->setFlag(QSGMaterial::Blending);
        simpleRectNode->setFlag(QSGNode::OwnsMaterial, true);
        QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        QSGGeometry::updateTexturedRectGeometry(geometry, boundingRect(), QRectF(0, 0, 1, 1));
        simpleRectNode->setGeometry(geometry);
        simpleRectNode->setFlag(QSGNode::OwnsGeometry, true);
    } else {
        QSGGeometry::updateTexturedRectGeometry(simpleRectNode->geometry(), boundingRect(), QRectF(0, 0, 1, 1));
        simpleRectNode->markDirty(QSGNode::DirtyGeometry);
    }
    simpleRectNode->setRect(boundingRect());
    return simpleRectNode;
}
