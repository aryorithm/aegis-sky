#include <QInternal>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "backend/app/StationApp.h"
#include "backend/comms/CoreClient.h"
#include "backend/video/VideoReceiver.h"


int main(int argc, char *argv[])
{


    QGuiApplication app(argc, argv);

    // Register the CoreClient type so QML can instantiate it
    qmlRegisterType<aegis::station::comms::CoreClient>("Aegis.Backend", 1, 0, "CoreClient");

    qmlRegisterType<aegis::station::video::VideoReceiver>("Aegis.Backend", 1, 0, "VideoReceiver");
    QQmlApplicationEngine engine;
    
    // Load Main QML
    const QUrl url(u"qrc:/Aegis/Station/src/frontend/Main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    
    engine.load(url);

    return app.exec();
}