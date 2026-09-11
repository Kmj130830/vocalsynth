#include <QApplication>
#include <QPalette>
#include <QSurfaceFormat>
#include "App/MainWindow.h"
#include "Utils/Logger.h"

namespace {
QString applicationStyleSheet()
{
    return QStringLiteral(R"CSS(
        QMainWindow, QWidget {
            background: #15171b;
            color: #d8dde4;
            font-family: "Segoe UI";
            font-size: 10pt;
        }
        QToolBar {
            background: #181a1e;
            border: 0;
            border-bottom: 1px solid #30343a;
            spacing: 2px;
            padding: 3px 6px;
        }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 3px;
            padding: 3px 7px;
            color: #bfc5cd;
        }
        QToolButton:hover { background: #242830; border-color: #343a43; }
        QToolButton:checked { background: #29384a; border-color: #45627f; color: #edf4fb; }
        QDockWidget {
            background: #15171b;
            color: #c8ced6;
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            background: #181a1e;
            border-bottom: 1px solid #30343a;
            padding: 5px 8px;
        }
        QListWidget {
            background: #121417;
            border: 0;
            outline: none;
            padding: 3px;
        }
        QListWidget::item { border: 1px solid transparent; border-radius: 3px; padding: 4px; }
        QListWidget::item:hover { background: #1e2228; }
        QListWidget::item:selected { background: #27384a; border-color: #415d79; }
        QComboBox, QDoubleSpinBox, QLineEdit, QPushButton {
            background: #20242a;
            color: #dce1e8;
            border: 1px solid #383e47;
            border-radius: 3px;
            padding: 3px 7px;
        }
        QComboBox:hover, QDoubleSpinBox:hover, QLineEdit:hover, QPushButton:hover { border-color: #4b5562; }
        QComboBox:focus, QDoubleSpinBox:focus, QLineEdit:focus, QPushButton:focus { border-color: #577897; }
        QPushButton:pressed { background: #293440; }
        QMenuBar { background: #181a1e; color: #cbd1d8; border-bottom: 1px solid #2e3339; }
        QMenuBar::item { padding: 5px 8px; }
        QMenuBar::item:selected { background: #272c33; }
        QMenu { background: #1b1e23; color: #d5dae1; border: 1px solid #353b44; padding: 3px; }
        QMenu::item { padding: 5px 24px 5px 10px; border-radius: 2px; }
        QMenu::item:selected { background: #2a3541; }
        QScrollBar:horizontal { height: 11px; background: #14161a; }
        QScrollBar:vertical { width: 11px; background: #14161a; }
        QScrollBar::handle:horizontal, QScrollBar::handle:vertical { background: #393f47; border-radius: 5px; min-width: 24px; min-height: 24px; }
        QScrollBar::handle:hover { background: #4b535e; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QStatusBar { background: #181a1e; color: #9da5af; border-top: 1px solid #2d3238; }
        #TransportBar { background: #181a1e; }
        #TransportPosition { color: #dce2e9; font-family: "Consolas"; font-size: 10pt; }
        #TransportPlay, #TransportStop { min-width: 46px; }
    )CSS");
}
}

int main(int argc, char *argv[])
{
    QSurfaceFormat::setDefaultFormat(QSurfaceFormat());
    QApplication app(argc, argv);
    app.setApplicationName("MyVocalSynth");
    app.setOrganizationName("MyVocalSynth");
    app.setApplicationVersion("0.1.0");
    app.setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(applicationStyleSheet());
    myvocal::Logger::initialize();
    try {
        myvocal::MainWindow window;
        window.show();
        return app.exec();
    } catch (const std::exception &e) {
        myvocal::Logger::error(QStringLiteral("Fatal exception: %1").arg(e.what()));
        return 1;
    }
}
