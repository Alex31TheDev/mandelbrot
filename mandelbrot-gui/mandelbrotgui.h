#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_mandelbrotgui.h"

class mandelbrotgui : public QMainWindow
{
    Q_OBJECT

public:
    mandelbrotgui(QWidget *parent = nullptr);
    ~mandelbrotgui();

private:
    Ui::mandelbrotguiClass ui;
};

