#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;
class ImageProvider;
class ImageView;
class MaskCanvas;

// One screen of the app: a prompt, options and result preview. In Generate
// mode it creates new images; in Edit mode it also shows a source photo the
// user can paint a mask on.
class ImagePanel : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Generate, Edit };

    explicit ImagePanel(Mode mode, QWidget *parent = nullptr);

    // The provider to send requests to; may be null when no key is set.
    void setProvider(ImageProvider *provider);

    // Edit mode: replace the source photo (clears any mask).
    void setSourceImage(const QImage &image);

signals:
    // Generate mode: the user wants to edit the shown result.
    void editRequested(const QImage &image);

private:
    QWidget *createSourceArea();
    void startOrCancel();
    void setBusy(bool busy);
    void showResults(const QList<QImage> &images);
    void showError(const QString &message);
    void setStatus(const QString &text, bool error = false);
    void selectResult(int index);
    void saveCurrent();
    void copyCurrent();
    void useCurrent();
    void openSource();
    void pasteSource();
    void updateElapsed();
    void updateMaskControls();

    const Mode m_mode;
    QPointer<ImageProvider> m_provider;
    QList<QImage> m_results;
    QElapsedTimer m_elapsed;

    QPlainTextEdit *m_prompt;
    QComboBox *m_size;
    QComboBox *m_quality;
    QSpinBox *m_count;
    QPushButton *m_runButton;
    QLabel *m_status;
    QProgressBar *m_progress;
    ImageView *m_view;
    QListWidget *m_thumbnails;
    QPushButton *m_saveButton;
    QPushButton *m_copyButton;
    QPushButton *m_useButton;
    QTimer *m_ticker;

    // Edit mode only.
    MaskCanvas *m_canvas = nullptr;
    QSlider *m_brushSize = nullptr;
    QPushButton *m_clearMaskButton = nullptr;
    QLabel *m_maskHint = nullptr;
};
