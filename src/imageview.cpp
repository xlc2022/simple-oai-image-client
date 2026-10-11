#include "imageview.h"

#include <QPainter>

ImageView::ImageView(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 200);
}

void ImageView::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void ImageView::setPlaceholderText(const QString &text)
{
    m_placeholder = text;
    update();
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());

    if (m_image.isNull()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap,
                         m_placeholder);
        return;
    }

    QSize target = m_image.size();
    // Shrink to fit, but never upscale past 100%.
    if (target.width() > width() || target.height() > height())
        target.scale(size(), Qt::KeepAspectRatio);
    QRect drawRect(QPoint(), target);
    drawRect.moveCenter(rect().center());
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(drawRect, m_image);
}
