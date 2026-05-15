#pragma once

#include <QQuickItem>
#include <mpv/client.h>
#include <mpv/render_gl.h>

class MpvItem : public QQuickItem
{
    Q_OBJECT

public:
    MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem();

    mpv_handle *mpv() const { return m_mpv; }

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *) override;

private:
    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_mpv_gl = nullptr;

    static void *get_proc_address(void *ctx, const char *name);
    static void on_update(void *ctx);
};
