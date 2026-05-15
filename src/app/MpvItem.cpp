#include "MpvItem.h"
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGRenderNode>
#include <QMetaObject>

#include <QTimer>

class MpvRenderNode : public QSGRenderNode
{
public:
    mpv_render_context *mpv_gl = nullptr;
    QSize size;

    RenderingFlags flags() const override {
        return BoundedRectRendering;
    }

    void render(const RenderState *) override {
        if (!mpv_gl)
            return;

        glViewport(0, 0, size.width(), size.height());

        mpv_opengl_fbo fbo{
            0,
            size.width(),
            size.height(),
            0
        };

        int flip = 1;

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        mpv_render_context_render(mpv_gl, params);
    }
};

MpvItem::MpvItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);

    m_mpv = mpv_create();

    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);

    mpv_set_option_string(m_mpv, "untimed", "yes");
    mpv_set_option_string(m_mpv, "profile", "low-latency");
    mpv_set_option_string(m_mpv, "framedrop", "vo");

#ifdef __ARM_ARCH
    mpv_set_option_string(m_mpv, "vo", "gpu-next");
    mpv_set_option_string(m_mpv, "gpu-api", "opengl");
    mpv_set_option_string(m_mpv, "hwdec", "no");
#else
    mpv_set_option_string(m_mpv, "vo", "libmpv");
#endif

    mpv_initialize(m_mpv);
}

MpvItem::~MpvItem()
{
    if (m_mpv_gl)
        mpv_render_context_free(m_mpv_gl);

    if (m_mpv)
        mpv_terminate_destroy(m_mpv);
}

void *MpvItem::get_proc_address(void *, const char *name)
{
    auto ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return nullptr;

    return reinterpret_cast<void*>(ctx->getProcAddress(name));
}

void MpvItem::on_update(void *ctx)
{
    MpvItem *self = static_cast<MpvItem*>(ctx);

    QMetaObject::invokeMethod(self, [self]() {

        if (!self->m_mpv_gl)
            return;

        uint64_t flags = mpv_render_context_update(self->m_mpv_gl);

        if (flags & MPV_RENDER_UPDATE_FRAME) {
            if (self->window())
                self->window()->update();
        }

    }, Qt::QueuedConnection);
}

QSGNode *MpvItem::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    auto n = static_cast<MpvRenderNode*>(node);

    if (!n) {
        n = new MpvRenderNode();

        if (!m_mpv_gl) {
            mpv_opengl_init_params gl_init{
                get_proc_address,
                nullptr
            };

            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
                {MPV_RENDER_PARAM_INVALID, nullptr}
            };

            mpv_render_context_create(&m_mpv_gl, m_mpv, params);

            mpv_set_option_string(m_mpv, "video-sync", "display-resample");
            mpv_set_option_string(m_mpv, "interpolation", "no");

            mpv_render_context_set_update_callback(m_mpv_gl, on_update, this);
        }

        n->mpv_gl = m_mpv_gl;
    }

    //n->size = QSize(int(width()), int(height()));

    qreal dpr = window() ? window()->devicePixelRatio() : 1.0;

    n->size = QSize(int(width() * dpr), int(height() * dpr));

    return n;
}
