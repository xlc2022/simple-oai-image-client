#pragma once

class QImage;
class QWidget;

// Shared Save… / Copy behaviour for the panels that show results.
namespace ImageActions {
// Asks for a file name (PNG or JPEG, defaulting to Pictures) and saves.
void saveWithDialog(QWidget *parent, const QImage &image);
void copyToClipboard(const QImage &image);
} // namespace ImageActions
