#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QLineEdit>
#include <QWidget>

#include "PaletteTypes.h"

class PaletteTimelineWidget final : public QWidget {
    Q_OBJECT

public:
    explicit PaletteTimelineWidget(QWidget *parent = nullptr);

    void setBlendEnds(bool blendEnds);
    void setStops(const std::vector<PaletteStop> &stops);
    [[nodiscard]] const std::vector<PaletteStop> &stops() const;
    [[nodiscard]] int selectedIndex() const;
    [[nodiscard]] int selectedCount() const;
    void setSelectedColor(const QColor &color);
    void addStop();
    void removeSelectedStops();
    void evenSpacing();

signals:
    void changed();
    void selectionChanged(int index);
    void editColorRequested(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    bool _blendEnds = true;
    std::vector<PaletteStop> _stops;
    int _nextId = 100;
    std::optional<int> _primarySelectedId;
    std::unordered_set<int> _selectedIds;
    std::optional<int> _draggingId;
    double _dragAnchorPos = 0.0;
    std::unordered_map<int, double> _dragStartPositions;
    QLineEdit *_positionEditor = nullptr;

    [[nodiscard]] QRect _gradientRect() const;
    [[nodiscard]] QPoint _stopPoint(double pos) const;
    [[nodiscard]] double _posFromPixel(double x) const;
    [[nodiscard]] QColor _sample(double pos) const;
    [[nodiscard]] int _hitTest(const QPoint &point) const;
    void _sortStops();
    [[nodiscard]] bool _isLockedIndex(int index) const;
    [[nodiscard]] bool _isLockedId(int id) const;
    [[nodiscard]] double _minimumStopSpacing() const;
    void _normalizeStopLayout();
    void _updatePositionEditor();
    void _commitPositionEdit();
    void _setSingleSelection(int id);
    void _emitChanged();
    void _emitSelectionChanged();
};
