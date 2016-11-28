// Default empty project template
#include <bb/cascades/Application>

#include <QLocale>
#include <QTranslator>
#include "applicationui.hpp"

// include JS Debugger / CS Profiler enabler
// this feature is enabled by default in the debug build only
#include <Qt/qdeclarativedebug.h>
#include "audioproc.h"
#include "picker.h"

using namespace bb::cascades;

Q_DECL_EXPORT int main(int argc, char **argv)
{

    // this is where the server is started etc
    Application app(argc, argv);

    fprintf( stderr, "%s:%d %s %s \n", __func__, __LINE__, __DATE__, __TIME__ );
    ap = new AudioProc();

    // localization support
    QTranslator translator;
    QString locale_string = QLocale().name();
    QString filename = QString( "CompressDistort_%1" ).arg( locale_string );
    if (translator.load(filename, "app/native/qm")) {
        app.installTranslator( &translator );
    }

    qmlRegisterType<ValuePickerProvider>("custom.pickers", 1, 0, "ValuePickerProvider");

    new ApplicationUI(&app);

    // we complete the transaction started in the app constructor and start the client event loop here
    int ret = Application::exec();
    delete ap;
    return ret;

    // when loop is exited the Application deletes the scene which deletes all its children (per qt rules for children)
}
