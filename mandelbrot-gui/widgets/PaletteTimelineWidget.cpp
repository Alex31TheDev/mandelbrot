#include "PaletteTimelineWidget.h"

#include <algorithm>
#include <cmath>

#include <QIntValidator>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

static const int handleWidth = 12;
static const int handleHeight = 12;
static const int handleHitHalfWidth = 14;
static const int handleHitTopPadding = 14;
static const int handleHitBottomPadding = 20;
static const int hitRepeatTolerance = 3;

PaletteTimelineWidget::PaletteTimelineWidget(QWidget *parent)
    : QWidget(parent) {
    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);

    _positionEditor = new QLineEdit(this);
    _positionEditor->setFixedWidth(54);
    _positionEditor->setAlignment(Qt::AlignCenter);
    _positionEditor->setValidator(new QIntValidator(0, 100, _positionEditor));
    _positionEditor->hide();

    QObject::connect(_positionEditor, &QLineEdit::textEdited, this,
        [this](const QString &) { _applyPositionEdit(false); });
    QObject::connect(_positionEditor, &QLineEdit::editingFinished, this,
        [this]() { _applyPositionEdit(true); });
}

void PaletteTimelineWidget::setBlendEnds(bool blendEnds) {
    _blendEnds = blendEnds;
    _normalizeStopLayout();
    _updatePositionEditor();
    update();
}

void PaletteTimelineWidget::setStops(const std::vector<PaletteStop> &stops) {
    _stops = stops;
    if (_stops.empty()) {
        _selectedIds.clear();
        _primarySelectedId.reset();
    } else if (!_primarySelectedId
        || !_selectedIds.contains(*_primarySelectedId)) {
        _setSingleSelection(_stops.front().id);
    }

    _sortStops();
    _normalizeStopLayout();
    _updatePositionEditor();
    update();
}

const std::vector<PaletteStop> &PaletteTimelineWidget::stops() const {
    return _stops;
}

int PaletteTimelineWidget::selectedIndex() const {
    if (!_primarySelectedId) return -1;

    for (int i = 0; i < static_cast<int>(_stops.size()); i++) {
        if (_stops[i].id == *_primarySelectedId) return i;
    }
    return -1;
}

int PaletteTimelineWidget::selectedCount() const {
    return static_cast<int>(_selectedIds.size());
}

void PaletteTimelineWidget::setSelectedColor(const QColor &color) {
    if (_selectedIds.empty()) return;

    for (auto &stop : _stops) {
        if (_selectedIds.contains(stop.id)) {
            stop.color = color;
        }
    }
    _emitChanged();
    update();
}

void PaletteTimelineWidget::addStop() {
    _sortStops();
    _normalizeStopLayout();

    PaletteStop stop;
    stop.id = ++_nextId;

    if (_stops.size() < 2) {
        stop.pos = 0.5;
    } else {
        double bestStart = 0.0;
        double bestEnd = _blendEnds ? _stops.front().pos + 1.0 : 1.0;
        double bestGap = -1.0;

        for (size_t i = 1; i < _stops.size(); i++) {
            const double start = _stops[i - 1].pos;
            const double end = _stops[i].pos;
            const double gap = end - start;
            if (gap > bestGap) {
                bestGap = gap;
                bestStart = start;
                bestEnd = end;
            }
        }

        if (_blendEnds && _stops.size() >= 2) {
            const double wrapStart = _stops.back().pos;
            const double wrapEnd = _stops.front().pos + 1.0;
            const double wrapGap = wrapEnd - wrapStart;
            if (wrapGap > bestGap) {
                bestGap = wrapGap;
                bestStart = wrapStart;
                bestEnd = wrapEnd;
            }
        }

        stop.pos = (bestStart + bestEnd) * 0.5;
        if (stop.pos >= 1.0) stop.pos -= 1.0;
    }

    stop.color = _sample(stop.pos);
    _stops.push_back(stop);
    _setSingleSelection(stop.id);
    _sortStops();
    _normalizeStopLayout();
    _emitSelectionChanged();
    _emitChanged();
    update();
}

void PaletteTimelineWidget::removeSelectedStops() {
    if (_selectedIds.empty()) return;
    if (static_cast<int>(_stops.size()) - selectedCount() < 2) return;

    _stops.erase(std::remove_if(_stops.begin(), _stops.end(),
        [this](const PaletteStop &stop) {
            return _selectedIds.contains(stop.id);
        }),
        _stops.end());

    _selectedIds.clear();
    if (_stops.empty()) {
        _primarySelectedId.reset();
    } else {
        _primarySelectedId = _stops.front().id;
        _selectedIds.insert(*_primarySelectedId);
    }

    _normalizeStopLayout();
    _emitSelectionChanged();
    _emitChanged();
    update();
}

void PaletteTimelineWidget::evenSpacing() {
    if (_stops.size() < 2) return;

    _sortStops();
    for (size_t i = 0; i < _stops.size(); i++) {
        if (_blendEnds) {
            _stops[i].pos
                = static_cast<double>(i) / static_cast<double>(_stops.size());
        } else {
            const double denom = static_cast<double>(_stops.size() - 1);
            _stops[i].pos = denom > 0.0 ? static_cast<double>(i) / denom : 0.0;
        }
    }

    _normalizeStopLayout();
    _emitChanged();
    update();
}

void PaletteTimelineWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    _updatePositionEditor();
}

void PaletteTimelineWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    const QRect grad = _gradientRect();
    QLinearGradient gradient(grad.topLeft(), grad.topRight());
    for (const auto &stop : _stops) {
        gradient.setColorAt(std::clamp(stop.pos, 0.0, 1.0), stop.color);
    }
    if (!_stops.empty()) {
        gradient.setColorAt(
            1.0, _blendEnds ? _stops.front().color : _stops.back().color);
    }

    painter.fillRect(grad, gradient);
    painter.setPen(palette().dark().color());
    painter.drawRect(grad.adjusted(0, 0, -1, -1));

    for (const auto &stop : _stops) {
        const QPoint handlePos = _stopPoint(stop.pos);
        QPainterPath path;
        path.moveTo(handlePos.x(), grad.bottom() + 4);
        path.lineTo(
            handlePos.x() - handleWidth / 2, grad.bottom() + 4 + handleHeight);
        path.lineTo(
            handlePos.x() + handleWidth / 2, grad.bottom() + 4 + handleHeight);
        path.closeSubpath();

        painter.fillPath(path, stop.color);
        painter.setPen(_selectedIds.contains(stop.id)
            ? QPen(Qt::black, 1.5)
            : QPen(palette().mid().color(), 1.0));
        painter.drawPath(path);
    }
}

void PaletteTimelineWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        const int hit = _hitTest(event->position().toPoint());
        const bool toggleSelection
            = event->modifiers().testFlag(Qt::ControlModifier);

        if (hit >= 0) {
            const int id = _stops[hit].id;
            if (toggleSelection) {
                if (_selectedIds.contains(id) && _selectedIds.size() > 1) {
                    _selectedIds.erase(id);
                    if (_primarySelectedId == id) {
                        _primarySelectedId = *_selectedIds.begin();
                    }
                } else {
                    _selectedIds.insert(id);
                    _primarySelectedId = id;
                }
            } else if (!_selectedIds.contains(id) || _selectedIds.size() != 1) {
                _setSingleSelection(id);
            }

            _draggingId.reset();
            _dragStartPositions.clear();
            if (!_isLockedId(id)) {
                _draggingId = id;
                _dragAnchorPos = _posFromPixel(event->position().x());
                for (const auto &stop : _stops) {
                    if (_selectedIds.contains(stop.id)
                        && !_isLockedId(stop.id)) {
                        _dragStartPositions[stop.id] = stop.pos;
                    }
                }
            }
            _emitSelectionChanged();
        } else if (_gradientRect().contains(event->position().toPoint())) {
            PaletteStop stop;
            stop.id = ++_nextId;
            stop.pos = _posFromPixel(event->position().x());
            stop.color = _sample(stop.pos);

            _stops.push_back(stop);
            _setSingleSelection(stop.id);
            _draggingId = stop.id;
            _dragAnchorPos = stop.pos;
            _dragStartPositions = { { stop.id, stop.pos } };
            _sortStops();
            _normalizeStopLayout();
            _emitSelectionChanged();
            _emitChanged();
        } else if (!toggleSelection) {
            _selectedIds.clear();
            _primarySelectedId.reset();
            _emitSelectionChanged();
        }
        update();
    }

    if (event->button() == Qt::RightButton) {
        const int hit = _hitTest(event->position().toPoint());
        if (hit >= 0) {
            if (!_selectedIds.contains(_stops[hit].id)) {
                _setSingleSelection(_stops[hit].id);
            }
            removeSelectedStops();
        }
    }
}

void PaletteTimelineWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!_draggingId || _dragStartPositions.empty()) return;

    const double targetPos = _posFromPixel(event->position().x());
    double delta = targetPos - _dragAnchorPos;
    double minDelta = -1.0;
    double maxDelta = 1.0;
    for (const auto &[id, pos] : _dragStartPositions) {
        (void)id;
        minDelta = std::max(minDelta, -pos);
        maxDelta = std::min(maxDelta, 1.0 - pos);
    }

    delta = minDelta > maxDelta ? minDelta
        : std::clamp(delta, minDelta, maxDelta);

    for (auto &stop : _stops) {
        const auto it = _dragStartPositions.find(stop.id);
        if (it != _dragStartPositions.end()) {
            stop.pos = it->second + delta;
        }
    }

    _sortStops();
    _emitChanged();
    _updatePositionEditor();
    update();
}

void PaletteTimelineWidget::mouseReleaseEvent(QMouseEvent *) {
    _draggingId.reset();
    _dragStartPositions.clear();
}

void PaletteTimelineWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    const int hit = _hitTest(event->position().toPoint());
    if (hit < 0) return;

    _setSingleSelection(_stops[hit].id);
    _emitSelectionChanged();
    emit editColorRequested(hit);
    update();
}

QRect PaletteTimelineWidget::_gradientRect() const {
    return rect().adjusted(12, 12, -12, -28);
}

QPoint PaletteTimelineWidget::_stopPoint(double pos) const {
    const QRect grad = _gradientRect();
    const int x
        = grad.left() + static_cast<int>(std::round(pos * grad.width()));
    return { std::clamp(x, grad.left(), grad.right()), grad.bottom() };
}

QRect PaletteTimelineWidget::_stopHitRect(double pos) const {
    const QPoint handle = _stopPoint(pos);
    return { handle.x() - handleHitHalfWidth,
        handle.y() - handleHitTopPadding, handleHitHalfWidth * 2 + 1,
        handleHitTopPadding + handleHitBottomPadding + 1 };
}

double PaletteTimelineWidget::_posFromPixel(double x) const {
    const QRect grad = _gradientRect();
    if (grad.width() <= 0) return 0.0;
    return std::clamp((x - grad.left()) / grad.width(), 0.0, 1.0);
}

QColor PaletteTimelineWidget::_sample(double pos) const {
    if (_stops.empty()) return Qt::white;

    std::vector<PaletteStop> sorted = _stops;
    std::sort(sorted.begin(), sorted.end(),
        [](const PaletteStop &a, const PaletteStop &b) {
            return a.pos < b.pos;
        });

    if (_blendEnds && sorted.size() >= 2) {
        if (pos < sorted.front().pos || pos > sorted.back().pos) {
            const double start = sorted.back().pos;
            const double end = sorted.front().pos + 1.0;
            const double samplePos = pos < sorted.front().pos ? pos + 1.0 : pos;
            const double span = std::max(end - start, 1e-6);
            const double t = (samplePos - start) / span;
            return QColor::fromRgbF(sorted.back().color.redF()
                + (sorted.front().color.redF() - sorted.back().color.redF())
                * t,
                sorted.back().color.greenF()
                + (sorted.front().color.greenF()
                    - sorted.back().color.greenF())
                * t,
                sorted.back().color.blueF()
                + (sorted.front().color.blueF()
                    - sorted.back().color.blueF())
                * t);
        }
    } else {
        if (pos <= sorted.front().pos) return sorted.front().color;
        if (pos >= sorted.back().pos) return sorted.back().color;
    }

    for (size_t i = 0; i + 1 < sorted.size(); i++) {
        if (pos < sorted[i].pos || pos > sorted[i + 1].pos) continue;
        const double span = std::max(sorted[i + 1].pos - sorted[i].pos, 1e-6);
        const double t = (pos - sorted[i].pos) / span;
        return QColor::fromRgbF(sorted[i].color.redF()
            + (sorted[i + 1].color.redF() - sorted[i].color.redF()) * t,
            sorted[i].color.greenF()
            + (sorted[i + 1].color.greenF() - sorted[i].color.greenF()) * t,
            sorted[i].color.blueF()
            + (sorted[i + 1].color.blueF() - sorted[i].color.blueF()) * t);
    }

    return sorted.back().color;
}

int PaletteTimelineWidget::_indexForId(int id) const {
    for (int i = 0; i < static_cast<int>(_stops.size()); i++) {
        if (_stops[i].id == id) return i;
    }
    return -1;
}

std::vector<int> PaletteTimelineWidget::_hitCandidates(
    const QPoint &point
) const {
    struct Candidate {
        int index = -1;
        int distance = 0;
        bool selected = false;
        int id = 0;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(_stops.size());
    for (int i = 0; i < static_cast<int>(_stops.size()); i++) {
        const QRect area = _stopHitRect(_stops[i].pos);
        if (!area.contains(point)) continue;

        const QPoint handle = _stopPoint(_stops[i].pos);
        const int dx = point.x() - handle.x();
        const int dy = point.y() - (handle.y() + 6);
        candidates.push_back({ i, dx * dx + dy * dy,
            _selectedIds.contains(_stops[i].id), _stops[i].id });
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate &a, const Candidate &b) {
            if (a.selected != b.selected) return a.selected > b.selected;
            if (a.distance != b.distance) return a.distance < b.distance;
            return a.id > b.id;
        });

    std::vector<int> indices;
    indices.reserve(candidates.size());
    for (const Candidate &candidate : candidates) {
        indices.push_back(candidate.index);
    }
    return indices;
}

int PaletteTimelineWidget::_hitTest(const QPoint &point) {
    const std::vector<int> candidates = _hitCandidates(point);
    if (candidates.empty()) {
        _lastHitCandidateIds.clear();
        _lastHitChosenId = -1;
        return -1;
    }

    const bool repeatedHit = !_lastHitCandidateIds.empty()
        && std::abs(point.x() - _lastHitPoint.x()) <= hitRepeatTolerance
        && std::abs(point.y() - _lastHitPoint.y()) <= hitRepeatTolerance
        && candidates.size() == _lastHitCandidateIds.size()
        && std::equal(candidates.begin(), candidates.end(),
            _lastHitCandidateIds.begin(),
            [this](int index, int id) { return _stops[index].id == id; });

    int chosenIndex = candidates.front();
    if (repeatedHit && candidates.size() > 1) {
        int previousOffset = -1;
        for (int i = 0; i < static_cast<int>(candidates.size()); i++) {
            if (_stops[candidates[i]].id == _lastHitChosenId) {
                previousOffset = i;
                break;
            }
        }
        if (previousOffset >= 0) {
            chosenIndex = candidates[(previousOffset + 1)
                % static_cast<int>(candidates.size())];
        }
    }

    _lastHitPoint = point;
    _lastHitCandidateIds.clear();
    _lastHitCandidateIds.reserve(candidates.size());
    for (const int candidate : candidates) {
        _lastHitCandidateIds.push_back(_stops[candidate].id);
    }
    _lastHitChosenId = _stops[chosenIndex].id;
    return chosenIndex;
}

void PaletteTimelineWidget::_sortStops() {
    std::sort(_stops.begin(), _stops.end(),
        [](const PaletteStop &a, const PaletteStop &b) {
            return a.pos < b.pos;
        });
}

bool PaletteTimelineWidget::_isLockedIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(_stops.size())) return false;
    if (index == 0) return true;
    if (!_blendEnds && index == static_cast<int>(_stops.size()) - 1)
        return true;
    return false;
}

bool PaletteTimelineWidget::_isLockedId(int id) const {
    for (int i = 0; i < static_cast<int>(_stops.size()); i++) {
        if (_stops[i].id == id) return _isLockedIndex(i);
    }
    return false;
}

double PaletteTimelineWidget::_minimumStopSpacing() const {
    return 0.0;
}

void PaletteTimelineWidget::_normalizeStopLayout() {
    if (_stops.empty()) return;

    _sortStops();
    _stops.front().pos = 0.0;
    const double minGap = _minimumStopSpacing();
    const int firstMovable = 1;
    const int lastMovable = _blendEnds ? static_cast<int>(_stops.size()) - 1
        : static_cast<int>(_stops.size()) - 2;

    if (!_blendEnds && _stops.size() >= 2) {
        _stops.back().pos = 1.0;
    }

    if (lastMovable >= firstMovable) {
        for (int i = firstMovable; i <= lastMovable; i++) {
            _stops[i].pos = std::max(_stops[i].pos, _stops[i - 1].pos + minGap);
        }

        const double upperBound = 1.0 - minGap;
        if (_stops[lastMovable].pos > upperBound) {
            _stops[lastMovable].pos = upperBound;
            for (int i = lastMovable - 1; i >= firstMovable; --i) {
                _stops[i].pos
                    = std::min(_stops[i].pos, _stops[i + 1].pos - minGap);
            }
        }

        if (_stops[firstMovable].pos < minGap) {
            const int movableCount = lastMovable - firstMovable + 1;
            if (movableCount > 0) {
                const double start = minGap;
                const double end = 1.0 - minGap;
                const double denom = movableCount + (_blendEnds ? 1.0 : 0.0);
                for (int i = 0; i < movableCount; i++) {
                    _stops[firstMovable + i].pos = start
                        + (end - start) * static_cast<double>(i + 1)
                        / std::max(1.0, denom);
                }
            }
        }
    }

    if (!_blendEnds && _stops.size() >= 2) {
        for (int i = 1; i + 1 < static_cast<int>(_stops.size()); i++) {
            const double minPos = _stops[i - 1].pos + minGap;
            const double maxPos = _stops[i + 1].pos - minGap;
            _stops[i].pos = minPos > maxPos
                ? minPos
                : std::clamp(_stops[i].pos, minPos, maxPos);
        }
        _stops.back().pos = 1.0;
    }
}

void PaletteTimelineWidget::_updatePositionEditor() {
    if (!_positionEditor) return;
    if (_selectedIds.size() != 1) {
        _positionEditor->hide();
        return;
    }

    const int index = selectedIndex();
    if (index < 0 || index >= static_cast<int>(_stops.size())) {
        _positionEditor->hide();
        return;
    }

    const QString text = QString::number(
        static_cast<int>(std::lround(_stops[index].pos * 100.0)));
    if (!_positionEditor->hasFocus()) {
        _positionEditor->setText(text);
    }

    const QRect grad = _gradientRect();
    const QPoint handle = _stopPoint(_stops[index].pos);
    const int x
        = std::clamp(handle.x() - _positionEditor->width() / 2, grad.left(),
            std::max(grad.left(), grad.right() - _positionEditor->width() + 1));
    const int y = std::max(0, grad.top() - _positionEditor->height() - 6);
    _positionEditor->move(x, y);
    _positionEditor->setReadOnly(_isLockedIndex(index));
    _positionEditor->show();
    _positionEditor->raise();
}

void PaletteTimelineWidget::_applyPositionEdit(bool finalCommit) {
    if (!_positionEditor || !_positionEditor->isVisible()) return;
    const int index = selectedIndex();
    if (index < 0 || index >= static_cast<int>(_stops.size())) return;
    if (_isLockedIndex(index)) {
        if (finalCommit) _updatePositionEditor();
        return;
    }

    bool ok = false;
    const int entered = _positionEditor->text().toInt(&ok);
    if (!ok) {
        if (finalCommit) _updatePositionEditor();
        return;
    }

    _stops[index].pos
        = std::clamp(static_cast<double>(entered) / 100.0, 0.0, 1.0);
    _sortStops();
    _normalizeStopLayout();
    _emitChanged();
    _emitSelectionChanged();
    update();
}

void PaletteTimelineWidget::_setSingleSelection(int id) {
    _selectedIds.clear();
    _selectedIds.insert(id);
    _primarySelectedId = id;
    _lastHitChosenId = id;
}

void PaletteTimelineWidget::_emitChanged() {
    emit changed();
}

void PaletteTimelineWidget::_emitSelectionChanged() {
    _updatePositionEditor();
    emit selectionChanged(selectedIndex());
}