#include "cam/SqlRepository.h"
#include "cam/Document.h"
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <stdexcept>
namespace cam {
namespace {
void check(bool ok,const QSqlQuery& q){if(!ok)throw std::runtime_error(q.lastError().text().toStdString());}
}
SqlRepository::SqlRepository():name_("cam-"+uuid()){}
SqlRepository::~SqlRepository(){if(db_.isValid()){db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(name_);}}
void SqlRepository::connect(const QString& connection) {
    if(!QSqlDatabase::isDriverAvailable("QODBC"))throw std::runtime_error("Qt QODBC plugin not available. Deploy sqldrivers/qsqlodbc.dll.");
    if(!db_.isValid())db_=QSqlDatabase::addDatabase("QODBC",name_);
    db_.close();db_.setDatabaseName(connection);
    if(!db_.open())throw std::runtime_error(db_.lastError().text().toStdString());
}
bool SqlRepository::connected()const{return db_.isOpen();}
QList<QPair<QString,QString>> SqlRepository::projects(){
    QSqlQuery q(db_);check(q.exec("EXEC cam.ProjectList"),q);QList<QPair<QString,QString>> out;
    while(q.next())out.append({q.value(0).toString(),q.value(1).toString()});return out;
}
QPair<QJsonObject,QByteArray> SqlRepository::load(const QString& id){
    QSqlQuery q(db_);q.prepare("EXEC cam.ProjectLoad @Id=?");q.addBindValue(id);check(q.exec(),q);
    if(!q.next())throw std::runtime_error("Project not found or access denied");
    auto p=QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();validateDocument(p);return {p,q.value(1).toByteArray()};
}
QByteArray SqlRepository::save(const QJsonObject& p,const QByteArray& expected){
    validateDocument(p);QSqlQuery q(db_);q.prepare("EXEC cam.ProjectSave @Id=?, @Name=?, @Document=?, @ExpectedVersion=?");
    q.addBindValue(p["id"].toString());q.addBindValue(p["name"].toString());
    q.addBindValue(QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)));
    q.addBindValue(expected.isEmpty()?QVariant(QMetaType::fromType<QByteArray>()):QVariant(expected));check(q.exec(),q);
    do {if(q.next())return q.value(0).toByteArray();} while(q.nextResult());
    throw std::runtime_error("Save returned no rowversion; reload to reconcile before retrying");
}
}
