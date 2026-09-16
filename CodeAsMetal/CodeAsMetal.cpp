/** @file Production entry point replacing the educational Win32 message loop.
 * QApplication owns the native event loop; the original example is preserved in
 * legacy/Win32Template. Exceptions are reported rather than escaping main(). */
#include "cam/MainWindow.h"
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <Standard_Failure.hxx>
#include <exception>
namespace cam { void installLogging(); }
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("CodeAsMetal");
    QCoreApplication::setApplicationName("CodeAsMetal");
    QCoreApplication::setApplicationVersion("1.0.0-preview.1");
    cam::installLogging();
    try {
        cam::MainWindow window;window.show();return app.exec();
    } catch(const Standard_Failure& e){QMessageBox::critical(nullptr,"CAD initialization",e.GetMessageString());}
      catch(const std::exception& e){QMessageBox::critical(nullptr,"Startup",QString::fromUtf8(e.what()));}
    return 1;
}
