#pragma once

#include <QImage>
#include <QWidget>

// Shows the photo being edited and lets the user paint over the area the model
// should change. Accepts dropped image files.
class MaskCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit MaskCanvas(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    QImage image() const { return m_image; }

    // Painting is only offered when the provider can use a mask.
    void setMaskingEnabled(bool enabled);
    void setBrushSize(int widgetPixels) { m_brushSize = widgetPixels; }

    bool hasMask() const { return m_hasStrokes; }
    void clearMask();
    // Mask in OpenAI's convention: same size as the image, opaque everywhere
    // except fully transparent where the user painted. Null if nothing painted.
    QImage mask() const;

    QSize sizeHint() const override { return {512, 512}; }

signals:
    void imageDropped(const QImage &image);
    void maskChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QRectF imageRect() const;
    QPointF toImage(const QPointF &widgetPos) const;
    void strokeTo(const QPointF &widgetPos);

    QImage m_image;
    QImage m_strokes; // ARGB, image-sized; painted pixels are opaque
    bool m_hasStrokes = false;
    bool m_maskingEnabled = true;
    bool m_painting = false;
    int m_brushSize = 40;
    QPointF m_lastImagePos;
    QPointF m_hoverPos{-1, -1};
};
