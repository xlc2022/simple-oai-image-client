#include "imageactions.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QStandardPaths>

namespace ImageActions {

void saveWithDialog(QWidget *parent, const QImage &image)
{
    if (image.isNull())
        return;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString suggested = QDir(dir).filePath(
        QStringLiteral("image-%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
    const QString pngFilter = QObject::tr("PNG image (*.png)");
    const QString jpegFilter = QObject::tr("JPEG image (*.jpg *.jpeg)");
    QString selectedFilter = pngFilter;
    QString path = QFileDialog::getSaveFileName(parent, QObject::tr("Save Image"), suggested,
                                                pngFilter + QStringLiteral(";;") + jpegFilter,
                                                &selectedFilter);
    if (path.isEmpty())
        return;

    // QImage::save picks the format from the suffix, so make it match the chosen filter.
    const bool wantJpeg = selectedFilter == jpegFilter;
    const QString suffix = QFileInfo(path).suffix().toLower();
    const bool isJpeg = suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg");
    if (wantJpeg != isJpeg || suffix.isEmpty()) {
        if (!suffix.isEmpty())
            path.chop(suffix.size() + 1);
        path += wantJpeg ? QStringLiteral(".jpg") : QStringLiteral(".png");
    }

    if (!image.save(path))
        QMessageBox::warning(parent, QObject::tr("Save Image"),
                             QObject::tr("Could not save to %1.").arg(path));
}

void copyToClipboard(const QImage &image)
{
    if (!image.isNull())
        QApplication::clipboard()->setImage(image);
}

} // namespace ImageActions
