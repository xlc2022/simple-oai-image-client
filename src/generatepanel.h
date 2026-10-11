#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QImage>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class ImageProvider;
class ImageView;

// Prompt box, options and result preview for generating new images.
class GeneratePanel : public QWidget
{
    Q_OBJECT

public:
    explicit GeneratePanel(QWidget *parent = nullptr);

    // The provider to send requests to; may be null when no key is set.
    void setProvider(ImageProvider *provider);

private:
    void startOrCancel();
    void setBusy(bool busy);
    void showResults(const QList<QImage> &images);
    void showError(const QString &message);
    void setStatus(const QString &text, bool error = false);
    void selectResult(int index);
    void saveCurrent();
    void copyCurrent();
    void updateElapsed();

    QPointer<ImageProvider> m_provider;
    QList<QImage> m_results;
    QElapsedTimer m_elapsed;

    QPlainTextEdit *m_prompt;
    QComboBox *m_size;
    QComboBox *m_quality;
    QSpinBox *m_count;
    QPushButton *m_generateButton;
    QLabel *m_status;
    QProgressBar *m_progress;
    ImageView *m_view;
    QListWidget *m_thumbnails;
    QPushButton *m_saveButton;
    QPushButton *m_copyButton;
    QTimer *m_ticker;
};
