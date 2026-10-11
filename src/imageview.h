#pragma once

#include <QImage>
#include <QWidget>

// Shows one image scaled to fit, keeping its aspect ratio, with a placeholder
// message when empty.
class ImageView : public QWidget
{
    Q_OBJECT

public:
    explicit ImageView(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    QImage image() const { return m_image; }
    void setPlaceholderText(const QString &text);

    QSize sizeHint() const override { return {512, 512}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;
    QString m_placeholder;
};
