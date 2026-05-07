#include "ViewportOverlayWidget.h"
#include "ui_ViewportOverlayWidget.h"

#include "ViewportGridOverlayWidget.h"
#include "ViewportStatusOverlayWidget.h"
#include "ViewportZoomOverlayWidget.h"

ViewportOverlayWidget::ViewportOverlayWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::ViewportOverlayWidget>()) {
    _ui->setupUi(this);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    _gridWidget = _ui->gridWidget;
    _statusWidget = _ui->statusWidget;
    _zoomWidget = _ui->zoomWidget;
    _syncChildGeometry();
    refreshOverlay();
}

ViewportOverlayWidget::~ViewportOverlayWidget() = default;

void ViewportOverlayWidget::setHost(ViewportHost *host) {
    _host = host;
    if (_statusWidget) {
        _statusWidget->setHost(_host);
    }
    refreshOverlay();
}

void ViewportOverlayWidget::refreshOverlay() {
    if (_gridWidget) {
        _gridWidget->refreshOverlay(_minimalUI);
    }

    if (_zoomWidget) {
        _zoomWidget->refreshOverlay(_minimalUI);
    }

    if (_statusWidget) {
        _statusWidget->refreshOverlay(_minimalUI);
    }
}

void ViewportOverlayWidget::cycleGridMode() {
    if (_gridWidget) {
        _gridWidget->cycleMode();
    }
    refreshOverlay();
}

void ViewportOverlayWidget::toggleMinimalUI() {
    _minimalUI = !_minimalUI;
    refreshOverlay();
}

void ViewportOverlayWidget::beginZoomSelection(const QPoint &origin) {
    if (_zoomWidget) {
        _zoomWidget->beginSelection(origin);
    }
    refreshOverlay();
}

void ViewportOverlayWidget::updateZoomSelection(const QPoint &current) {
    if (_zoomWidget) {
        _zoomWidget->updateSelection(current);
    }
    refreshOverlay();
}

bool ViewportOverlayWidget::scaleZoomSelection(double factor) {
    const bool scaled = _zoomWidget && _zoomWidget->scaleSelection(factor, rect());
    if (scaled) {
        refreshOverlay();
    }
    return scaled;
}

void ViewportOverlayWidget::clearZoomSelection() {
    if (_zoomWidget) {
        _zoomWidget->clearSelection();
    }
    refreshOverlay();
}

QRect ViewportOverlayWidget::zoomSelectionRect() const {
    return _zoomWidget ? _zoomWidget->selectionRect() : QRect();
}

bool ViewportOverlayWidget::hasZoomSelection() const {
    return _zoomWidget && _zoomWidget->hasSelection();
}

void ViewportOverlayWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    _syncChildGeometry();
}

void ViewportOverlayWidget::_syncChildGeometry() {
    const QRect area = rect();
    if (_gridWidget) {
        _gridWidget->setGeometry(area);
    }
    if (_zoomWidget) {
        _zoomWidget->setGeometry(area);
    }
    if (_statusWidget) {
        _statusWidget->setGeometry(area);
    }
}
