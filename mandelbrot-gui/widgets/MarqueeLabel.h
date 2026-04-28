#pragma once

#include <QColor>
#include <QLabel>
#include <QTimer>

class MarqueeLabel final : public QLabel {
    Q_OBJECT
    Q_PROPERTY(bool marqueeEnabled READ marqueeEnabled WRITE setMarqueeEnabled)
    Q_PROPERTY(int marqueeIntervalMs READ marqueeIntervalMs WRITE setMarqueeIntervalMs)
    Q_PROPERTY(QString marqueeSeparator READ marqueeSeparator WRITE setMarqueeSeparator)
    Q_PROPERTY(bool emphasisEnabled READ emphasisEnabled WRITE setEmphasisEnabled)
    Q_PROPERTY(QColor emphasisColor READ emphasisColor WRITE setEmphasisColor)

public:
    explicit MarqueeLabel(QWidget* parent = nullptr);

    [[nodiscard]] bool marqueeEnabled() const { return _marqueeEnabled; }
    void setMarqueeEnabled(bool enabled);

    [[nodiscard]] int marqueeIntervalMs() const { return _marqueeTimer.interval(); }
    void setMarqueeIntervalMs(int intervalMs);

    [[nodiscard]] const QString& marqueeSeparator() const {
        return _marqueeSeparator;
    }
    void setMarqueeSeparator(const QString& separator);

    [[nodiscard]] bool emphasisEnabled() const { return _emphasisEnabled; }
    void setEmphasisEnabled(bool enabled);

    [[nodiscard]] QColor emphasisColor() const { return _emphasisColor; }
    void setEmphasisColor(const QColor& color);

    void setSourceText(const QString& text);
    [[nodiscard]] const QString& sourceText() const { return _sourceText; }

    void setLinkPath(const QString& linkPath);
    [[nodiscard]] const QString& linkPath() const { return _linkPath; }

    void setSource(const QString& text, const QString& linkPath = QString());
    [[nodiscard]] bool wouldMarquee(const QString& text) const;

signals:
    void layoutWidthChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QString _sourceText;
    QString _linkPath;
    QString _marqueeSeparator = "     ";
    QColor _emphasisColor { 215, 80, 80 };
    QTimer _marqueeTimer;
    int _marqueeOffset = 0;
    bool _marqueeEnabled = true;
    bool _emphasisEnabled = false;

    [[nodiscard]] bool _shouldMarquee() const;
    void _syncTimer();
    void _updateDisplayedText();
};
