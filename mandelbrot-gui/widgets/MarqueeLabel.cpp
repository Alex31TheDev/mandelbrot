#include "MarqueeLabel.h"

#include <algorithm>

#include <QDesktopServices>
#include <QEvent>
#include <QResizeEvent>
#include <QUrl>

MarqueeLabel::MarqueeLabel(QWidget *parent)
    : QLabel(parent) {
    _marqueeTimer.setInterval(125);
    QObject::connect(&_marqueeTimer, &QTimer::timeout, this, [this]() {
        if (_sourceText.isEmpty()) return;
        _marqueeOffset++;
        _updateDisplayedText();
        });
    QObject::connect(this, &QLabel::linkActivated, this,
        [](const QString &link) { QDesktopServices::openUrl(QUrl(link)); });
    QObject::connect(this, &QLabel::linkHovered, this, [this](const QString &link) {
        if (link.isEmpty()) {
            unsetCursor();
        } else {
            setCursor(Qt::PointingHandCursor);
        }
        });
}

void MarqueeLabel::setMarqueeEnabled(bool enabled) {
    if (_marqueeEnabled == enabled) return;
    _marqueeEnabled = enabled;
    _syncTimer();
    _updateDisplayedText();
}

void MarqueeLabel::setMarqueeIntervalMs(int intervalMs) {
    _marqueeTimer.setInterval(std::max(1, intervalMs));
}

void MarqueeLabel::setMarqueeSeparator(const QString &separator) {
    const QString effectiveSeparator = separator.isEmpty() ? " " : separator;
    if (_marqueeSeparator == effectiveSeparator) return;
    _marqueeSeparator = effectiveSeparator;
    _marqueeOffset = 0;
    _updateDisplayedText();
}

void MarqueeLabel::setEmphasisEnabled(bool enabled) {
    if (_emphasisEnabled == enabled) return;
    _emphasisEnabled = enabled;
    _updateDisplayedText();
}

void MarqueeLabel::setEmphasisColor(const QColor &color) {
    if (_emphasisColor == color) return;
    _emphasisColor = color;
    _updateDisplayedText();
}

void MarqueeLabel::setSourceText(const QString &text) {
    if (_sourceText == text) {
        _updateDisplayedText();
        return;
    }

    _sourceText = text;
    _marqueeOffset = 0;
    _updateDisplayedText();
}

void MarqueeLabel::setLinkPath(const QString &linkPath) {
    if (_linkPath == linkPath) {
        _updateDisplayedText();
        return;
    }

    _linkPath = linkPath;
    _updateDisplayedText();
}

void MarqueeLabel::setSource(const QString &text, const QString &linkPath) {
    const bool sameText = _sourceText == text;
    const bool sameLink = _linkPath == linkPath;
    if (sameText && sameLink) {
        _updateDisplayedText();
        return;
    }

    _sourceText = text;
    _linkPath = linkPath;
    _marqueeOffset = 0;
    _updateDisplayedText();
}

bool MarqueeLabel::wouldMarquee(const QString &text) const {
    if (!_marqueeEnabled || text.isEmpty()) return false;

    const int availableWidth = std::max(1, contentsRect().width());
    const QFontMetrics metrics(font());
    return metrics.horizontalAdvance(text) > availableWidth;
}

void MarqueeLabel::resizeEvent(QResizeEvent *event) {
    QLabel::resizeEvent(event);
    _updateDisplayedText();
    if (event->size().width() != event->oldSize().width()) {
        emit layoutWidthChanged();
    }
}

void MarqueeLabel::changeEvent(QEvent *event) {
    QLabel::changeEvent(event);
    switch (event->type()) {
        case QEvent::EnabledChange:
        case QEvent::FontChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            _updateDisplayedText();
            break;
        default:
            break;
    }
}

bool MarqueeLabel::_shouldMarquee() const {
    return wouldMarquee(_sourceText);
}

void MarqueeLabel::_syncTimer() {
    if (_shouldMarquee()) {
        if (!_marqueeTimer.isActive()) _marqueeTimer.start();
        return;
    }

    if (_marqueeTimer.isActive()) _marqueeTimer.stop();
    _marqueeOffset = 0;
}

void MarqueeLabel::_updateDisplayedText() {
    _syncTimer();

    QString shownText = _sourceText;
    if (_shouldMarquee() && !_sourceText.isEmpty()) {
        const QString padded = _sourceText + _marqueeSeparator;
        const int wrappedOffset = _marqueeOffset % padded.size();
        shownText = padded.mid(wrappedOffset) + padded.left(wrappedOffset);
    }

    const QString escapedText = shownText.toHtmlEscaped();
    const QString emphasisStyle
        = _emphasisEnabled
        ? QString(" style=\"color:%1;\"").arg(_emphasisColor.name())
        : QString();

    if (!_linkPath.isEmpty()) {
        const QString href = QUrl::fromLocalFile(_linkPath).toString();
        setText(QString("<a href=\"%1\"%2>%3</a>")
            .arg(href, emphasisStyle, escapedText));
    } else if (_emphasisEnabled) {
        setText(QString("<span style=\"color:%1;\">%2</span>")
            .arg(_emphasisColor.name(), escapedText));
    } else {
        setText(escapedText);
    }
}