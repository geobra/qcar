#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQmlContext>
#include <QtQml>
#include <stdlib.h>


#include "MpvItem.h"
#include "CameraClient.h"

int main(int argc, char *argv[])
{
    setlocale(LC_NUMERIC, "C");  // for mpv
    setenv("LC_NUMERIC", "C", 1);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
	    
    QGuiApplication app(argc, argv);

    qmlRegisterType<MpvItem>("WifiCam", 1, 0, "MpvItem");

    CameraClient client;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("cameraClient", &client);

    engine.load(QUrl(QStringLiteral("qrc:/WifiCam/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
