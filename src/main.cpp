#include <QApplication>
#include <QSurfaceFormat>
#include "App/MainWindow.h"
#include "Utils/Logger.h"

int main(int argc, char *argv[])
{
    QSurfaceFormat::setDefaultFormat(QSurfaceFormat());
    QApplication app(argc, argv);
    app.setApplicationName("MyVocalSynth");
    app.setOrganizationName("MyVocalSynth");
    app.setApplicationVersion("0.1.0");
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
