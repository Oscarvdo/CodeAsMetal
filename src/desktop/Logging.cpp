/** Qt diagnostics to a bounded local log. This sink receives Qt warnings only;
 * project inputs, ODBC credentials and CAD bytes are never explicitly logged.
 * The mutex protects messages emitted from the import worker and UI thread. */
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtLogging>
namespace cam {
void installLogging(){
    static QMutex mutex;
    static const QString directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/logs";
    QDir().mkpath(directory);
    qInstallMessageHandler([](QtMsgType type,const QMessageLogContext&,const QString& message){
        QMutexLocker lock(&mutex);const auto path=directory+"/CodeAsMetal.log";
        if(QFile(path).size()>2*1024*1024){QFile::remove(path+".1");QFile::rename(path,path+".1");}
        QFile file(path);if(file.open(QIODevice::WriteOnly|QIODevice::Append))file.write((QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+" ["+QString::number(type)+"] "+message+"\n").toUtf8());
    });
}
}
