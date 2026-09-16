#pragma once
#include <QSqlDatabase>
#include <QJsonObject>
#include <QByteArray>
#include <QStringList>
namespace cam {
/** UI-thread-affine SQL Server repository using QODBC and integrated security.
 * No connection strings are written to project files or logs. Rowversion is opaque.
 * Server permissions, not UI conventions, protect audit and estimate snapshots. */
class SqlRepository final {
public:
    SqlRepository();
    ~SqlRepository();
    SqlRepository(const SqlRepository&)=delete;
    SqlRepository& operator=(const SqlRepository&)=delete;
    void connect(const QString& connectionString);
    bool connected() const;
    /** List ID and display name of accessible projects. */
    QList<QPair<QString,QString>> projects();
    /** Return the aggregate plus its expected rowversion token. */
    QPair<QJsonObject,QByteArray> load(const QString& id);
    /** Create if token empty; otherwise compare-and-swap. Conflict never overwrites. */
    QByteArray save(const QJsonObject&,const QByteArray& expected);
private:
    QString name_;
    QSqlDatabase db_;
};
}
