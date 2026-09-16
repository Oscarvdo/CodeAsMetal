#pragma once
#include "cam/Viewer.h"
#include "cam/SqlRepository.h"
#include "cam/Cad.h"
#include <QMainWindow>
#include <QJsonObject>
#include <QTableWidget>
#include <QListWidget>
#include <QTextBrowser>
#include <QTabWidget>
#include <QFutureWatcher>
#include <functional>
namespace cam {
/** Application coordinator: widgets collect intent, pure core executes rules,
 * CAD adapter imports, repositories persist. No SQL/CAD calculation in widgets. */
class MainWindow final:public QMainWindow {
public:
    MainWindow();
protected:
    void closeEvent(QCloseEvent*)override;
private:
    QJsonObject project_;
    int revision_{-1};
    QString localPath_,actor_;
    QByteArray sqlVersion_;
    bool dirty_{},busy_{};
    SqlRepository sql_;
    Viewer* viewer_{};
    QListWidget* revisions_{};
    QTabWidget* tabs_{};
    QTextBrowser *summary_{},*costs_{};
    QTableWidget *features_{},*findings_{},*operations_{},*issues_{},*rates_{},*estimates_{};
    QJsonObject current()const;
    void replaceCurrent(QJsonObject);
    void changed(const QString& action,const QJsonObject& before={},const QJsonObject& after={});
    void refresh();
    void recompute(QJsonObject&);
    void guarded(const std::function<void()>&);
    bool abandon();
    void newDocument();
    void openDocument();
    void saveDocument(bool choosePath=false);
    void importRevision();
    void loadCad(bool addRevision,const QString& path,double scale=1,const QString& label={});
    void showCad();
    void editFeature(bool add);
    void editOperation(bool add);
    void removeOperation();
    void moveOperation(int delta);
    void editIssue(bool add);
    void addRate();
    void editInputs();
    void editRules();
    void issueEstimate();
    void compareRevisions();
    void exportReport(bool pdf);
    void connectSql();
    void openSql();
    void saveSql();
    void recover();
    QString recoveryPath()const;
};
}
