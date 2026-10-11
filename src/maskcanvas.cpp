#include "maskcanvas.h"

#include <QBitmap>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QImageReader>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

MaskCanvas::MaskCanvas(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 200);
    setAcceptDrops(true);
    setMouseTracking(true);
}

void MaskCanvas::setImage(const QImage &image)
{
    m_image = image.convertToFormat(QImage::Format_ARGB32);
    m_strokes = QImage(m_image.size(), QImage::Format_ARGB32);
    m_strokes.fill(Qt::transparent);
    m_hasStrokes = false;
    m_undo.clear();
    m_outline = QPainterPath();
    emit maskChanged();
    update();
}

void MaskCanvas::setMaskingEnabled(bool enabled)
{
    m_maskingEnabled = enabled;
    setCursor(enabled && !m_image.isNull() ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void MaskCanvas::clearMask()
{
    if (!m_hasStrokes)
        return;
    pushUndo();
    m_strokes.fill(Qt::transparent);
    strokesChanged();
}

void MaskCanvas::undo()
{
    if (m_undo.isEmpty())
        return;
    m_strokes = m_undo.takeLast();
    strokesChanged();
}

void MaskCanvas::pushUndo()
{
    m_undo.append(m_strokes);
    if (m_undo.size() > kUndoLimit)
        m_undo.removeFirst();
}

void MaskCanvas::strokesChanged()
{
    // Recompute what's painted (erasing can remove everything) and the outline.
    const QImage alpha = m_strokes.createAlphaMask();
    const QRegion region(QBitmap::fromImage(alpha));
    m_hasStrokes = !region.isEmpty();
    m_outline = QPainterPath();
    if (m_hasStrokes)
        m_outline.addRegion(region);
    m_outline = m_outline.simplified();
    emit maskChanged();
    update();
}

QImage MaskCanvas::mask() const
{
    if (!m_hasStrokes)
        return {};
    QImage mask(m_image.size(), QImage::Format_ARGB32);
    mask.fill(Qt::white);
    QPainter painter(&mask);
    // Punch out painted pixels: destination alpha becomes 0 where strokes are opaque.
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.drawImage(0, 0, m_strokes);
    return mask;
}

QRectF MaskCanvas::imageRect() const
{
    if (m_image.isNull())
        return {};
    QSizeF target = m_image.size();
    if (target.width() > width() || target.height() > height())
        target.scale(size(), Qt::KeepAspectRatio);
    QRectF r(QPointF(), target);
    r.moveCenter(QRectF(rect()).center());
    return r;
}

QPointF MaskCanvas::toImage(const QPointF &widgetPos) const
{
    const QRectF r = imageRect();
    const qreal scale = m_image.width() / r.width();
    return (widgetPos - r.topLeft()) * scale;
}

void MaskCanvas::strokeTo(const QPointF &widgetPos)
{
    const QPointF pos = toImage(widgetPos);
    const qreal scale = m_image.width() / imageRect().width();
    QPainter painter(&m_strokes);
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_erasing)
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.setPen(QPen(Qt::red, m_brushSize * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    // A zero-length line draws nothing, so a single click needs an explicit dot.
    if (pos == m_lastImagePos)
        painter.drawPoint(pos);
    else
        painter.drawLine(m_lastImagePos, pos);
    m_lastImagePos = pos;
    if (!m_hasStrokes && !m_erasing) {
        m_hasStrokes = true;
        emit maskChanged();
    }
    update();
}

void MaskCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());

    if (m_image.isNull()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Drop a photo here, or use Open… or Paste."));
        return;
    }

    const QRectF r = imageRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(r, m_image);
    if (m_hasStrokes) {
        painter.setOpacity(0.45);
        painter.drawImage(r, m_strokes);
        painter.setOpacity(1.0);
        // Outline the region (white + dashed black) so it reads on photos of any colour.
        // Refreshed when a stroke ends; during a stroke the tint alone shows progress.
        if (!m_outline.isEmpty()) {
            painter.save();
            painter.translate(r.topLeft());
            painter.scale(r.width() / m_image.width(), r.height() / m_image.height());
            QPen white(Qt::white, 2);
            white.setCosmetic(true);
            QPen dashed(Qt::black, 1, Qt::DashLine);
            dashed.setCosmetic(true);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(white);
            painter.drawPath(m_outline);
            painter.setPen(dashed);
            painter.drawPath(m_outline);
            painter.restore();
        }
    }

    // Brush outline under the cursor.
    if (m_maskingEnabled && r.contains(m_hoverPos)) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.drawEllipse(m_hoverPos, m_brushSize / 2.0, m_brushSize / 2.0);
        painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
        painter.drawEllipse(m_hoverPos, m_brushSize / 2.0, m_brushSize / 2.0);
    }
}

void MaskCanvas::mousePressEvent(QMouseEvent *event)
{
    if (!m_maskingEnabled || m_image.isNull())
        return;
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
        return;
    pushUndo();
    m_painting = true;
    m_erasing = event->button() == Qt::RightButton;
    m_lastImagePos = toImage(event->position());
    strokeTo(event->position());
}

void MaskCanvas::mouseMoveEvent(QMouseEvent *event)
{
    m_hoverPos = event->position();
    if (m_painting)
        strokeTo(event->position());
    else if (m_maskingEnabled && !m_image.isNull())
        update();
}

void MaskCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_painting && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
        m_painting = false;
        m_erasing = false;
        strokesChanged();
    }
}

void MaskCanvas::leaveEvent(QEvent *)
{
    m_hoverPos = QPointF(-1, -1);
    update();
}

bool MaskCanvas::isImageFile(const QUrl &url)
{
    if (!url.isLocalFile())
        return false;
    const QByteArray suffix = QFileInfo(url.toLocalFile()).suffix().toLower().toLatin1();
    return QImageReader::supportedImageFormats().contains(suffix);
}

void MaskCanvas::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mime = event->mimeData();
    bool ok = mime->hasImage();
    for (const QUrl &url : mime->urls())
        ok = ok || isImageFile(url);
    if (ok)
        event->acceptProposedAction();
}

void MaskCanvas::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    QImage image;
    if (mime->hasImage())
        image = qvariant_cast<QImage>(mime->imageData());
    for (const QUrl &url : mime->urls()) {
        if (!image.isNull())
            break;
        if (isImageFile(url)) {
            QImageReader reader(url.toLocalFile());
            reader.setAutoTransform(true);
            image = reader.read();
        }
    }
    if (image.isNull())
        return;
    event->acceptProposedAction();
    emit imageDropped(image);
}
