#include "cam/MainWindow.h"
#include "cam/Document.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QSignalBlocker>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSaveFile>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextDocument>
#include <QPrinter>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cam {
namespace {
QString q(const std::string& x){return QString::fromStdString(x);}
struct Field {QString key,label;QStringList choices;};
// Transactional form: validation happens before any aggregate is modified.
std::optional<QJsonObject> form(QWidget* parent,const QString& title,const QJsonObject& initial,
                               const std::vector<Field>& fields) {
    QDialog dlg(parent);dlg.setWindowTitle(title);dlg.resize(600,300);auto* layout=new QFormLayout(&dlg);
    std::vector<QWidget*> editors;
    for(const auto& f:fields) {
        QString text=initial[f.key].isDouble()?QString::number(initial[f.key].toDouble(),'g',15):initial[f.key].toString();
        if(!f.choices.isEmpty()){auto* c=new QComboBox;c->addItems(f.choices);c->setCurrentText(text);editors.push_back(c);layout->addRow(f.label,c);}
        else {auto* e=new QLineEdit(text);editors.push_back(e);layout->addRow(f.label,e);}
    }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addRow(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dlg,&QDialog::reject);
    if(dlg.exec()!=QDialog::Accepted)return {};
    QJsonObject out=initial;
    for(std::size_t i=0;i<fields.size();++i){auto* e=qobject_cast<QLineEdit*>(editors[i]);out[fields[i].key]=e?e->text().trimmed():qobject_cast<QComboBox*>(editors[i])->currentText();}return out;
}
double number(const QJsonObject& o,const char* key){bool ok=false;double x=o[key].toString().toDouble(&ok);if(!ok||!std::isfinite(x)||x<0)throw std::invalid_argument(std::string(key)+": enter a finite nonnegative number (decimal point)");return x;}
void numeric(QJsonObject& o,const char* key,bool optional=false){if(optional&&o[key].toString().isEmpty())o[key]=QJsonValue::Null;else o[key]=number(o,key);}
void require(const QJsonObject& o,const QStringList& keys){for(const auto& k:keys)if(o[k].toString().isEmpty())throw std::invalid_argument((k+" is required").toStdString());}
QTableWidget* table(const QStringList& headers){auto* t=new QTableWidget(0,headers.size());t->setHorizontalHeaderLabels(headers);t->setEditTriggers(QAbstractItemView::NoEditTriggers);t->setSelectionBehavior(QAbstractItemView::SelectRows);t->setSelectionMode(QAbstractItemView::SingleSelection);t->horizontalHeader()->setStretchLastSection(true);t->verticalHeader()->hide();t->setAlternatingRowColors(true);return t;}
void row(QTableWidget* t,const QStringList& values){int r=t->rowCount();t->insertRow(r);for(int i=0;i<values.size();++i)t->setItem(r,i,new QTableWidgetItem(values[i]));}
QString jsonText(const QJsonObject& o){return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented));}
QWidget* page(QTableWidget* t,const QString& help){auto* w=new QWidget;auto* v=new QVBoxLayout(w);auto* l=new QLabel(help);l->setWordWrap(true);v->addWidget(l);v->addWidget(t);return w;}
QString amount(const QJsonValue& v){return v.isDouble()?QString::number(v.toDouble(),'f',2):"MISSING";}
struct ImportResult{std::shared_ptr<CadModel> model;QString error;};
}
MainWindow::MainWindow(){
    setWindowTitle("CodeAsMetal · Engineering Workbench");resize(1500,920);
    QSettings settings;actor_=settings.value("actor").toString();
    if(actor_.isEmpty()){bool ok=false;actor_=QInputDialog::getText(this,"Engineering identity","Your name (audit attribution):",QLineEdit::Normal,qEnvironmentVariable("USERNAME"),&ok).trimmed();if(actor_.isEmpty())actor_="Unspecified engineer";settings.setValue("actor",actor_);}
    project_=newProject("Untitled project");
    auto* splitter=new QSplitter;revisions_=new QListWidget;revisions_->setMaximumWidth(230);splitter->addWidget(revisions_);
    auto* right=new QSplitter(Qt::Vertical);viewer_=new Viewer;right->addWidget(viewer_);tabs_=new QTabWidget;right->addWidget(tabs_);right->setSizes({470,350});splitter->addWidget(right);splitter->setStretchFactor(1,1);setCentralWidget(splitter);
    summary_=new QTextBrowser;tabs_->addTab(summary_,"Product / geometry");
    features_=table({"ID","Type","State","Faces","Diameter mm","Depth mm","Width mm","Radius mm"});
    tabs_->addTab(page(features_,"B-Rep surface records and engineering features. Hole candidates require confirmation. Double-click to classify and document evidence."),"Features");
    findings_=table({"Feature","Rule","Version","Severity","Measured","Threshold","Explanation"});tabs_->addTab(page(findings_,"Configurable screening rules. An empty list does not certify manufacturability."),"DFM");
    operations_=table({"ID","Operation","Setup","Machine","Tool","Features","Cycle min/part","Setup min/batch","Reason"});tabs_->addTab(page(operations_,"Plan order is significant. Times are engineering estimates; charge setup once per setup, not once per feature. Rates must exclude labor."),"Manufacturing");
    costs_=new QTextBrowser;tabs_->addTab(costs_,"Should-cost");
    issues_=table({"ID","Geometry","Severity","Owner","Status","Comment","Recommendation"});tabs_->addTab(page(issues_,"Issue anchors apply only to this CAD hash. Review manually on each revision."),"Review");
    rates_=table({"ID","Key","Value","Currency","From","Until","Source"});tabs_->addTab(page(rates_,"Customer data only. material:<grade> per kg; machine:<id>, labor:<category>, inspection:<method> per hour. Add a new effective date to supersede a rate."),"Customer rates");
    estimates_=table({"ID","Issued UTC","Actor","Completeness %","Batch known subtotal","Currency"});tabs_->addTab(page(estimates_,"Immutable issued snapshots. Double-click to inspect original inputs and results; current tariffs do not recalculate them."),"Issued estimates");
    auto action=[&](QMenu* menu,const QString& title,auto fn,const QKeySequence& key=QKeySequence{}){auto* a=menu->addAction(title);a->setShortcut(key);connect(a,&QAction::triggered,this,[this,fn]{if(!busy_)guarded(fn);});return a;};
    auto* file=menuBar()->addMenu("File");action(file,"New project",[this]{newDocument();},QKeySequence::New);action(file,"Open project…",[this]{openDocument();},QKeySequence::Open);
    action(file,"Save project",[this]{saveDocument();},QKeySequence::Save);action(file,"Save project as…",[this]{saveDocument(true);});action(file,"Recover autosave…",[this]{recover();});file->addSeparator();
    auto* import=action(file,"Import CAD as revision…",[this]{importRevision();},QKeySequence("Ctrl+I"));
    action(file,"Reload / locate CAD",[this]{showCad();});
    action(file,"Attach drawing PDF…",[this]{auto r=current();auto path=QFileDialog::getOpenFileName(this,"Attach drawing",{},"PDF (*.pdf)");if(path.isEmpty())return;r["drawing"]=path;r["drawingSha256"]=hashFile(path);replaceCurrent(r);changed("Drawing attached");refresh();});
    action(file,"Generate sample CAD…",[this]{const auto d=QFileDialog::getExistingDirectory(this,"Sample directory");if(d.isEmpty())return;generateSamples(d);QMessageBox::information(this,"Samples","Created block.step, holes.step, holes-mm.stl. Import one as a revision.");});
    action(file,"Export report HTML…",[this]{exportReport(false);});action(file,"Export report PDF…",[this]{exportReport(true);});
    auto* engineering=menuBar()->addMenu("Engineering");action(engineering,"Product and cost inputs…",[this]{editInputs();});action(engineering,"DFM rules for current revision…",[this]{editRules();});
    action(engineering,"Add engineering feature…",[this]{editFeature(true);});action(engineering,"Edit selected feature…",[this]{editFeature(false);});
    action(engineering,"Suggest preliminary plan",[this]{auto r=current();if(!r["operations"].toArray().isEmpty())throw std::runtime_error("Plan already exists. Add/edit operations without replacing its history.");std::vector<Feature> f;for(auto x:r["features"].toArray())f.push_back(featureFromJson(x.toObject()));QJsonArray a;for(const auto& op:propose(f))a.append(toJson(op));r["operations"]=a;replaceCurrent(r);changed("Preliminary plan suggested");refresh();});
    action(engineering,"Add operation…",[this]{editOperation(true);});action(engineering,"Edit selected operation…",[this]{editOperation(false);});action(engineering,"Remove selected operation",[this]{removeOperation();});
    action(engineering,"Move operation up",[this]{moveOperation(-1);});action(engineering,"Move operation down",[this]{moveOperation(1);});
    action(engineering,"Add review issue…",[this]{editIssue(true);});action(engineering,"Edit selected issue…",[this]{editIssue(false);});action(engineering,"Add effective-dated rate…",[this]{addRate();});
    action(engineering,"Issue estimate snapshot…",[this]{issueEstimate();});action(engineering,"Compare revisions…",[this]{compareRevisions();});
    auto* view=menuBar()->addMenu("View");action(view,"Fit",[this]{viewer_->fit();},QKeySequence("F"));
    const QStringList names={"Isometric","Top","Front","Right"};for(int i=0;i<names.size();++i)action(view,names[i],[this,i]{viewer_->standardView(i);});
    action(view,"Select faces",[this]{viewer_->selectionMode(2);});action(view,"Select edges",[this]{viewer_->selectionMode(1);});action(view,"Select solids",[this]{viewer_->selectionMode(3);});
    auto* db=menuBar()->addMenu("SQL Server");action(db,"Connect…",[this]{connectSql();});action(db,"Open server project…",[this]{openSql();});action(db,"Save to server",[this]{saveSql();});
    auto* help=menuBar()->addMenu("Help");action(help,"About / scope",[this]{QMessageBox::information(this,"CodeAsMetal V1","Based on your VS2026 template. C++23 / Qt6 / OCCT / SQL Server.\n\nAutomatic: STEP topology, measurements, surface classification, conservative cylindrical cavity candidates.\n\nEngineer classification: through/blind holes, pockets, slots, chamfers, fillets, thin walls.\n\nNo CAM, machining certification or automatic drawing/PMI interpretation. See README and acceptance checklist.");});
    auto* bar=addToolBar("Workflow");bar->addAction(import);auto* fit=bar->addAction("Fit");connect(fit,&QAction::triggered,this,[this]{guarded([this]{viewer_->fit();});});
    connect(revisions_,&QListWidget::currentRowChanged,this,[this](int i){if(busy_||i==revision_)return;revision_=i;viewer_->clear();refresh();if(i>=0)guarded([this]{showCad();});});
    connect(features_,&QTableWidget::cellDoubleClicked,this,[this](int,int){guarded([this]{editFeature(false);});});
    connect(features_,&QTableWidget::cellClicked,this,[this](int row,int){guarded([this,row]{const auto a=current()["features"].toArray();if(row<a.size()){auto faces=a[row].toObject()["faces"].toArray();if(!faces.isEmpty())viewer_->highlightFace(faces[0].toInt());}});});
    viewer_->faceSelected=[this](int face){for(int i=0;i<features_->rowCount();++i){const auto a=current()["features"].toArray()[i].toObject()["faces"].toArray();for(auto x:a)if(x.toInt()==face){features_->selectRow(i);statusBar()->showMessage("Selected face "+QString::number(face)+" (revision-local)");return;}}};
    connect(operations_,&QTableWidget::cellDoubleClicked,this,[this](int,int){guarded([this]{editOperation(false);});});
    connect(issues_,&QTableWidget::cellDoubleClicked,this,[this](int,int){guarded([this]{editIssue(false);});});
    connect(estimates_,&QTableWidget::cellDoubleClicked,this,[this](int index,int){guarded([this,index]{auto a=current()["estimates"].toArray();if(index>=a.size())return;QDialog d(this);d.resize(900,650);d.setWindowTitle("Issued estimate — original snapshot");auto* l=new QVBoxLayout(&d);auto* t=new QTextBrowser;t->setPlainText(jsonText(a[index].toObject()));l->addWidget(t);d.exec();});});
    auto* timer=new QTimer(this);timer->setInterval(30000);connect(timer,&QTimer::timeout,this,[this]{if(dirty_&&!busy_)guarded([this]{writeDocument(recoveryPath(),project_);statusBar()->showMessage("Recovery saved · "+actor_,3000);});});timer->start();refresh();
    const auto paths=QDir(QFileInfo(recoveryPath()).absolutePath()).entryList({"*.cam.json"},QDir::Files);
    if(!paths.isEmpty())statusBar()->showMessage("Recovery documents available under File > Recover autosave.");
}
void MainWindow::guarded(const std::function<void()>& f){try{f();}catch(const Standard_Failure& e){QMessageBox::critical(this,"CAD error",e.GetMessageString());}catch(const std::exception& e){QMessageBox::critical(this,"CodeAsMetal",QString::fromUtf8(e.what()));}}
QJsonObject MainWindow::current()const{auto a=project_["revisions"].toArray();if(revision_<0||revision_>=a.size())throw std::runtime_error("Import or select a CAD revision first");return a[revision_].toObject();}
void MainWindow::replaceCurrent(QJsonObject r){auto a=project_["revisions"].toArray();if(revision_<0||revision_>=a.size())throw std::runtime_error("No revision selected");a[revision_]=r;project_["revisions"]=a;}
void MainWindow::changed(const QString& action,const QJsonObject& before,const QJsonObject& after){dirty_=true;auto a=project_["audit"].toArray();a.append(QJsonObject{{"id",uuid()},{"utc",nowUtc()},{"actor",actor_},{"action",action},{"revision",revision_>=0?current()["id"]:QJsonValue()},{"before",before},{"after",after}});project_["audit"]=a;setWindowModified(true);}
void MainWindow::recompute(QJsonObject& r){std::vector<Feature> f;for(auto x:r["features"].toArray())f.push_back(featureFromJson(x.toObject()));r["findings"]=findingsJson(evaluate(f,rulesFromJson(r["rules"].toObject())));r["dfmUtc"]=nowUtc();}
QString MainWindow::recoveryPath()const{const auto d=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/recovery";QDir().mkpath(d);return d+"/"+project_["id"].toString()+".cam.json";}
bool MainWindow::abandon(){if(!dirty_)return true;auto b=QMessageBox::question(this,"Unsaved changes","Save this project before continuing?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);if(b==QMessageBox::Cancel)return false;if(b==QMessageBox::Save){saveDocument();return !dirty_;}return true;}
void MainWindow::closeEvent(QCloseEvent* e){if(busy_){e->ignore();return;}bool close=false;guarded([&]{close=abandon();});if(close)e->accept();else e->ignore();}
void MainWindow::newDocument(){if(!abandon())return;bool ok;const auto name=QInputDialog::getText(this,"New project","Project name",QLineEdit::Normal,{},&ok).trimmed();if(!ok||name.isEmpty())return;project_=newProject(name);revision_=-1;localPath_.clear();sqlVersion_.clear();viewer_->clear();dirty_=false;refresh();}
void MainWindow::openDocument(){if(!abandon())return;const auto path=QFileDialog::getOpenFileName(this,"Open project",{},"CodeAsMetal (*.cam.json)");if(path.isEmpty())return;auto p=readDocument(path);project_=p;localPath_=path;sqlVersion_.clear();revision_=p["revisions"].toArray().isEmpty()?-1:0;dirty_=false;viewer_->clear();refresh();if(revision_>=0)showCad();}
void MainWindow::saveDocument(bool choose){QString path=localPath_;if(choose||path.isEmpty())path=QFileDialog::getSaveFileName(this,"Save project",path.isEmpty()?"project.cam.json":path,"CodeAsMetal (*.cam.json)");if(path.isEmpty())return;writeDocument(path,project_);localPath_=path;dirty_=false;setWindowModified(false);QFile::remove(recoveryPath());statusBar()->showMessage("Saved "+path,5000);}
void MainWindow::recover(){if(!abandon())return;const auto path=QFileDialog::getOpenFileName(this,"Recover project",QFileInfo(recoveryPath()).absolutePath(),"Recovery (*.cam.json)");if(path.isEmpty())return;project_=readDocument(path);localPath_.clear();sqlVersion_.clear();revision_=project_["revisions"].toArray().isEmpty()?-1:0;dirty_=true;viewer_->clear();refresh();if(revision_>=0)showCad();}
void MainWindow::importRevision(){
    const auto path=QFileDialog::getOpenFileName(this,"Import CAD",{},"CAD (*.step *.stp *.stl)");if(path.isEmpty())return;bool ok;
    auto label=QInputDialog::getText(this,"Revision","Unique revision label",QLineEdit::Normal,"Rev "+QString::number(project_["revisions"].toArray().size()+1),&ok).trimmed();if(!ok||label.isEmpty())return;
    for(auto x:project_["revisions"].toArray())if(x.toObject()["label"].toString()==label)throw std::runtime_error("Revision label already exists");
    double scale=1;if(QFileInfo(path).suffix().toLower()=="stl"){auto units=QInputDialog::getItem(this,"STL units","STL does not define units. Select source units:",{"Millimeters","Inches","Meters"},0,false,&ok);if(!ok)return;scale=units=="Inches"?25.4:units=="Meters"?1000:1;}
    loadCad(true,path,scale,label);
}
void MainWindow::loadCad(bool add,const QString& path,double scale,const QString& label){
    busy_=true;auto* progress=new QProgressDialog("Reading and analyzing CAD…",{},0,0,this);progress->setWindowModality(Qt::ApplicationModal);progress->setCancelButton(nullptr);progress->setMinimumDuration(0);progress->show();
    auto* watcher=new QFutureWatcher<ImportResult>(this);
    connect(watcher,&QFutureWatcher<ImportResult>::finished,this,[this,watcher,progress,add,label,scale]{
        const auto result=watcher->result();watcher->deleteLater();progress->close();progress->deleteLater();busy_=false;
        guarded([&]{if(!result.error.isEmpty())throw std::runtime_error(result.error.toStdString());const auto& m=*result.model;
          if(add){QJsonArray f;for(const auto& x:m.features)f.append(toJson(x));QJsonObject r{{"id",uuid()},{"label",label},{"path",m.sourcePath},{"sha256",m.sha256},{"stlScaleToMm",scale},{"importedUtc",nowUtc()},{"engine","cad-1.0"},{"kernelVersion",m.kernelVersion},{"geometry",toJson(m.geometry)},{"features",f},{"rules",project_["rules"]},{"warning",m.warning},{"operations",QJsonArray{}},{"issues",QJsonArray{}},{"estimates",QJsonArray{}}};recompute(r);auto a=project_["revisions"].toArray();a.append(r);project_["revisions"]=a;revision_=a.size()-1;changed("CAD revision imported");}
          else if(m.kernelVersion!=current()["kernelVersion"].toString())throw std::runtime_error("CAD kernel version changed. Reimport as a new revision to avoid invalid face identities.");
          else if(m.sha256!=current()["sha256"].toString())throw std::runtime_error("CAD hash changed. Import as a NEW revision; old analysis is preserved.");
          viewer_->display(m.shape);refresh();statusBar()->showMessage(m.warning.isEmpty()?"CAD loaded · millimeters":m.warning);
        });
    });
    watcher->setFuture(QtConcurrent::run([path,scale]{ImportResult r;try{r.model=importCad(path,scale);}catch(const std::exception& e){r.error=QString::fromUtf8(e.what());}catch(...){r.error="Unexpected CAD import failure";}return r;}));
}
void MainWindow::showCad(){auto r=current();QString path=r["path"].toString();if(!QFileInfo::exists(path)){path=QFileDialog::getOpenFileName(this,"Locate revision CAD (hash must match)",{},"CAD (*.step *.stp *.stl)");if(path.isEmpty())return;if(hashFile(path)!=r["sha256"].toString())throw std::runtime_error("Located file does not match revision SHA-256");auto before=r;r["path"]=path;replaceCurrent(r);changed("CAD reference relocated",before,r);}viewer_->clear();loadCad(false,path,r["stlScaleToMm"].toDouble(1));}
void MainWindow::refresh(){
    setWindowTitle("CodeAsMetal · "+project_["name"].toString()+"[*]");setWindowModified(dirty_);
    {QSignalBlocker b(revisions_);revisions_->clear();for(auto x:project_["revisions"].toArray())revisions_->addItem(x.toObject()["label"].toString());revisions_->setCurrentRow(revision_);}
    for(auto* t:{features_,findings_,operations_,issues_,rates_,estimates_})t->setRowCount(0);
    for(auto v:project_["rates"].toArray()){auto r=v.toObject();row(rates_,{r["id"].toString(),r["key"].toString(),QString::number(r["value"].toDouble(),'g',12),r["currency"].toString(),r["from"].toString(),r["to"].toString(),r["source"].toString()});}
    if(revision_<0){summary_->setHtml("<h2>CodeAsMetal Engineering Workbench</h2><p>1. Import a STEP file as a revision.<br>2. Review geometry and classify features.<br>3. Build a manufacturing plan.<br>4. Enter your shop rates and engineering time estimates.<br>5. Review issues and issue an estimate snapshot.</p><p>File → Generate sample CAD creates test parts.</p>");costs_->clear();return;}
    auto r=current();auto g=r["geometry"].toObject();
    summary_->setPlainText("REVISION: "+r["label"].toString()+"\nCAD: "+r["path"].toString()+"\nSHA-256: "+r["sha256"].toString()+"\nIMPORTED UTC: "+r["importedUtc"].toString()+"\nUNITS: mm, mm², mm³\n"+r["warning"].toString()+"\n\n"+jsonText(g)+"\nDRAWING: "+r["drawing"].toString()+"\nDRAWING HASH: "+r["drawingSha256"].toString()+"\n\nDrawing/PMI interpretation is manual. Stock mass is customer supplied; net CAD volume is not stock volume.");
    for(auto v:r["features"].toArray()){auto f=v.toObject();QStringList ids;for(auto x:f["faces"].toArray())ids<<QString::number(x.toInt());row(features_,{f["id"].toString(),f["kind"].toString(),f["confirmed"].toBool()?"Confirmed":"Candidate",ids.join(","),amount(f["diameter"]),amount(f["depth"]),amount(f["width"]),amount(f["radius"])});}
    for(auto v:r["findings"].toArray()){auto f=v.toObject();row(findings_,{f["feature"].toString(),f["rule"].toString(),f["version"].toString(),f["severity"].toString(),QString::number(f["measured"].toDouble(),'g',10),QString::number(f["threshold"].toDouble(),'g',10),f["explanation"].toString()});}
    for(auto v:r["operations"].toArray()){auto o=v.toObject();row(operations_,{o["id"].toString(),o["kind"].toString(),o["setup"].toString(),o["machine"].toString(),o["tool"].toString(),o["featureIds"].toString(),o["timeKnown"].toBool()?amount(o["minutes"]):"UNKNOWN",o["setupKnown"].toBool()?amount(o["setupMinutes"]):"UNKNOWN",o["overrideReason"].toString()});}
    for(auto v:r["issues"].toArray()){auto i=v.toObject();row(issues_,{i["id"].toString(),i["anchor"].toString(),i["severity"].toString(),i["owner"].toString(),i["status"].toString(),i["comment"].toString(),i["recommendation"].toString()});}
    for(auto v:r["estimates"].toArray()){auto e=v.toObject(),result=e["result"].toObject();row(estimates_,{e["id"].toString(),e["issuedUtc"].toString(),e["actor"].toString(),amount(result["completeness"]),amount(result["knownBatchTotal"]),e["currency"].toString()});}
    const auto estimateResult=estimate(costInput(project_,r));
    QString h="<h2>"+QString(estimateResult.complete?"Complete cost inputs":"Partial cost estimate")+"</h2><p>Currency: "+project_["currency"].toString().toHtmlEscaped()+" · Quantity: "+QString::number(project_["quantity"].toInt())+" · Input completeness: "+QString::number(estimateResult.completeness,'f',1)+"%</p><table cellpadding='5'><tr><th>Batch category</th><th>Amount</th></tr>";
    for(const auto& l:estimateResult.lines)h+="<tr><td>"+q(l.category)+"</td><td>"+(l.amount?QString::number(*l.amount,'f',2):"MISSING")+"</td></tr>";
    h+="</table><h3>Known batch subtotal: "+QString::number(estimateResult.knownBatchTotal,'f',2)+"</h3><p>Known subtotal per part: "+QString::number(estimateResult.knownUnitTotal,'f',2)+"</p><p>Completeness is not confidence. Times require engineering input. Review tool access, drawing requirements and capacity.</p>";
    for(const auto& l:estimateResult.lines)h+="<p><b>"+q(l.category)+"</b>: "+q(l.basis).toHtmlEscaped()+"</p>";costs_->setHtml(h);
    for(auto* t:{features_,findings_,operations_,issues_,rates_,estimates_})t->resizeColumnsToContents();
}
void MainWindow::editFeature(bool add){
    auto r=current();if(r["geometry"].toObject()["mesh"].toBool())throw std::runtime_error("STL does not support engineering feature attribution in this version; use STEP.");
    auto a=r["features"].toArray();int index=features_->currentRow();if(!add&&(index<0||index>=a.size()))throw std::runtime_error("Select a feature first");
    Feature f;if(add){f.id="ENG-"+uuid().left(8).toStdString();f.kind="Pocket";}else f=featureFromJson(a[index].toObject());
    auto before=toJson(f),initial=before;QStringList faces;for(int i:f.faces)faces<<QString::number(i);initial["facesText"]=faces.join(",");initial["state"]=f.confirmed?"Confirmed":"Candidate";
    auto edited=form(this,"Engineering feature (measurements in mm)",initial,{{"kind","Type",{"Through hole","Blind hole","Hole candidate","Pocket","Slot","Chamfer","Fillet","Thin wall","Planar face","Cylindrical surface","Conical surface","Toroidal surface","Other surface"}},
      {"facesText","Associated face IDs (comma separated)",{}},{"diameter","Diameter mm",{}},{"depth","Depth mm",{}},{"width","Width / wall thickness mm",{}},{"radius","Internal radius mm",{}},{"state","State",{"Candidate","Confirmed"}},{"evidence","Engineering evidence / reason",{}}});
    if(!edited)return;auto o=*edited;require(o,{"evidence"});for(const auto* k:{"diameter","depth","width","radius"})numeric(o,k);
    QJsonArray faceIds;for(auto token:o["facesText"].toString().split(',',Qt::SkipEmptyParts)){bool ok=false;int i=token.trimmed().toInt(&ok);if(!ok||i<1||i>r["geometry"].toObject()["faces"].toInt())throw std::invalid_argument("Face ID outside current geometry");faceIds.append(i);}if(faceIds.isEmpty())throw std::invalid_argument("Associate at least one face");
    o["faces"]=faceIds;o["confirmed"]=o["state"].toString()=="Confirmed";o.remove("state");o.remove("facesText");
    const auto kind=o["kind"].toString();if(o["confirmed"].toBool()&&(kind=="Through hole"||kind=="Blind hole")&&(o["diameter"].toDouble()<=0||o["depth"].toDouble()<=0))throw std::invalid_argument("Confirmed hole needs diameter and depth > 0");
    if(kind=="Thin wall"&&o["width"].toDouble()<=0)throw std::invalid_argument("Thin-wall feature needs measured thickness > 0");
    o["actor"]=actor_;o["changedUtc"]=nowUtc();if(add)a.append(o);else a[index]=o;r["features"]=a;recompute(r);replaceCurrent(r);changed(add?"Feature added":"Feature classified / overridden",before,o);refresh();
}
void MainWindow::editOperation(bool add){
    auto r=current();auto a=r["operations"].toArray();int index=operations_->currentRow();if(!add&&(index<0||index>=a.size()))throw std::runtime_error("Select an operation first");
    Operation op;if(add){op.id="OP-"+uuid().left(8).toStdString();op.kind="Milling";op.basis="Engineer-created operation";}else op=operationFromJson(a[index].toObject());
    auto before=toJson(op),initial=before;if(!op.timeKnown)initial["minutes"]=QJsonValue::Null;if(!op.setupKnown)initial["setupMinutes"]=QJsonValue::Null;
    auto edited=form(this,"Operation — blank time means unknown",initial,{{"kind","Operation",{"Milling","Drilling","Inspection","Stock preparation","Deburring"}},{"setup","Setup ID",{}},{"machine","Machine ID (matches machine: rate)",{}},{"tool","Tool reference",{}},{"featureIds","Feature IDs (comma separated)",{}},{"minutes","Cycle minutes per part",{}},{"setupMinutes","Setup minutes per batch (0 if none)",{}},{"overrideReason","Reason / time estimation basis",{}}});
    if(!edited)return;auto o=*edited;require(o,{"overrideReason"});
    QStringList known;for(auto f:r["features"].toArray())known<<f.toObject()["id"].toString();for(auto id:o["featureIds"].toString().split(',',Qt::SkipEmptyParts))if(!known.contains(id.trimmed()))throw std::invalid_argument("Operation references an unknown feature ID");
    const bool cycleKnown=!o["minutes"].toString().isEmpty(),setupKnown=!o["setupMinutes"].toString().isEmpty();
    const double cycle=cycleKnown?number(o,"minutes"):0,setup=setupKnown?number(o,"setupMinutes"):0;
    op.kind=o["kind"].toString().toStdString();op.setup=o["setup"].toString().toStdString();op.machine=o["machine"].toString().toStdString();op.tool=o["tool"].toString().toStdString();op.featureIds=o["featureIds"].toString().toStdString();
    if(cycleKnown)overrideTime(op,cycle,o["overrideReason"].toString().toStdString(),actor_.toStdString(),nowUtc().toStdString());
    else {op.minutes=0;op.timeKnown=false;op.overrideReason=o["overrideReason"].toString().toStdString();op.actor=actor_.toStdString();op.changedUtc=nowUtc().toStdString();}
    op.setupMinutes=setup;op.setupKnown=setupKnown;const auto after=toJson(op);if(add)a.append(after);else a[index]=after;r["operations"]=a;replaceCurrent(r);changed(add?"Operation added":"Operation overridden",before,after);refresh();
}
void MainWindow::removeOperation(){auto r=current();auto a=r["operations"].toArray();int i=operations_->currentRow();if(i<0||i>=a.size())throw std::runtime_error("Select an operation");bool ok;auto reason=QInputDialog::getText(this,"Remove operation","Reason",QLineEdit::Normal,{},&ok).trimmed();if(!ok)return;if(reason.isEmpty())throw std::invalid_argument("Reason required");auto before=a[i].toObject();a.removeAt(i);r["operations"]=a;replaceCurrent(r);changed("Operation removed: "+reason,before);refresh();}
void MainWindow::moveOperation(int delta){auto r=current();auto a=r["operations"].toArray();int i=operations_->currentRow(),j=i+delta;if(i<0||j<0||j>=a.size())return;auto moved=a[i];a.removeAt(i);a.insert(j,moved);r["operations"]=a;replaceCurrent(r);changed("Operation reordered",{{"from",i},{"to",j},{"id",moved.toObject()["id"]}});refresh();operations_->selectRow(j);}
void MainWindow::editIssue(bool add){
    auto r=current();auto a=r["issues"].toArray();int i=issues_->currentRow();if(!add&&(i<0||i>=a.size()))throw std::runtime_error("Select an issue");
    QJsonObject initial=add?QJsonObject{{"id","ISS-"+uuid().left(8)},{"anchor","Revision"},{"severity","Warning"},{"owner",actor_},{"status","Open"}}:a[i].toObject();
    QStringList anchors={"Revision"};for(auto f:r["features"].toArray())anchors<<f.toObject()["id"].toString();
    auto edited=form(this,"Engineering review",initial,{{"anchor","Geometry anchor",anchors},{"severity","Severity",{"Info","Warning","Major"}},{"owner","Owner",{}},{"status","Status",{"Open","In review","Resolved","Accepted risk"}},{"comment","Comment / decision rationale",{}},{"recommendation","Recommendation",{}}});if(!edited)return;auto o=*edited;require(o,{"owner","comment"});o["sha256"]=r["sha256"];o["actor"]=actor_;o["changedUtc"]=nowUtc();if(add)o["createdUtc"]=nowUtc();if(add)a.append(o);else a[i]=o;r["issues"]=a;replaceCurrent(r);changed(add?"Review issue added":"Review decision updated",initial,o);refresh();
}
void MainWindow::addRate(){
    QJsonObject initial{{"key","machine:VMC01"},{"currency",project_["currency"]},{"from",QDate::currentDate().toString(Qt::ISODate)}};
    auto edited=form(this,"Customer rate / historical successor",initial,{{"key","Key: material:, machine:, labor:, inspection:",{}},{"value","Value per kg (material) or per hour",{}},{"currency","Currency ISO code",{}},{"from","Effective from YYYY-MM-DD",{}},{"to","Effective until YYYY-MM-DD (blank = ongoing)",{}},{"source","Customer / supplier source reference",{}}});if(!edited)return;
    auto o=*edited;numeric(o,"value");o["id"]=uuid();o["currency"]=o["currency"].toString().toUpper();
    const auto key=o["key"].toString();if(!(key.startsWith("material:")||key.startsWith("machine:")||key.startsWith("labor:")||key.startsWith("inspection:"))||key.endsWith(':'))throw std::invalid_argument("Use a supported rate key with a nonempty ID");
    auto rates=project_["rates"].toArray();QJsonObject old;
    // Only close an ongoing prior interval; no deletion or in-place repricing.
    for(int i=0;i<rates.size();++i){auto prior=rates[i].toObject();if(prior["key"]==o["key"]&&prior["currency"]==o["currency"]&&prior["to"].toString().isEmpty()&&prior["from"].toString()<o["from"].toString()){old=prior;prior["to"]=o["from"];rates[i]=prior;}}
    rates.append(o);std::vector<Rate> parsed;for(auto x:rates)parsed.push_back(rateFromJson(x.toObject()));validateRates(parsed);
    project_["rates"]=rates;changed("Effective-dated customer rate added",old,o);refresh();
}
void MainWindow::editInputs(){
    auto initial=project_;auto edited=form(this,"Product and costing inputs — blank means missing",initial,{{"name","Project / part name",{}},{"material","Material grade / ID",{}},{"quantity","Batch quantity",{}},{"currency","Currency ISO code",{}},{"asOf","Rate valuation date YYYY-MM-DD",{}},{"stockMassKg","Purchased stock mass kg PER PART",{}},{"labor","Labor category",{}},{"inspector","Inspection method/category",{}},{"toolingPerPart","Tooling cost per part",{}},{"logisticsPerBatch","Logistics cost per batch",{}},{"overheadPercent","Overhead % of all direct categories",{}}});if(!edited)return;auto p=*edited;require(p,{"name","currency","asOf"});numeric(p,"quantity");for(auto key:{"stockMassKg","toolingPerPart","logisticsPerBatch","overheadPercent"})numeric(p,key,true);p["currency"]=p["currency"].toString().toUpper();validateDocument(p);
    QJsonObject before,after;for(auto k:{"name","material","quantity","currency","asOf","stockMassKg","labor","inspector","toolingPerPart","logisticsPerBatch","overheadPercent"}){before[k]=project_[k];after[k]=p[k];}project_=p;changed("Product / cost inputs changed",before,after);refresh();
}
void MainWindow::editRules(){auto r=current();auto before=r["rules"].toObject();auto edited=form(this,"DFM configuration (current revision)",before,{{"version","New rule-set version",{}},{"maxHoleRatio","Maximum hole depth / diameter",{}},{"minWall","Minimum wall mm",{}},{"minRadius","Minimum internal radius mm",{}}});if(!edited)return;auto o=*edited;for(auto k:{"maxHoleRatio","minWall","minRadius"})numeric(o,k);rulesFromJson(o);if(o!=before&&o["version"]==before["version"])throw std::invalid_argument("Increment the rule version when thresholds change");r["rules"]=o;recompute(r);replaceCurrent(r);project_["rules"]=o;changed("DFM rule-set changed",before,o);refresh();}
void MainWindow::issueEstimate(){
    auto r=current();const auto source=r["path"].toString();if(hashFile(source)!=r["sha256"].toString())throw std::runtime_error("Source CAD hash mismatch. Import as a new revision.");
    if(!r["drawing"].toString().isEmpty()&&hashFile(r["drawing"].toString())!=r["drawingSha256"].toString())throw std::runtime_error("Attached drawing changed; reattach and review before issuing.");
    const auto result=estimate(costInput(project_,r));
    bool ok;auto reason=QInputDialog::getText(this,"Issue estimate snapshot",result.complete?"Engineering review note (inputs complete; accuracy still requires review)":"PARTIAL estimate. Explain missing inputs and intended use:",QLineEdit::Normal,{},&ok).trimmed();if(!ok)return;if(reason.isEmpty())throw std::invalid_argument("Review note required");
    auto inputs=project_;inputs.remove("revisions");inputs.remove("audit");auto revisionInputs=r;revisionInputs.remove("estimates");
    QJsonObject snapshot{{"id",uuid()},{"issuedUtc",nowUtc()},{"actor",actor_},{"reviewNote",reason},{"currency",project_["currency"]},{"projectInputs",inputs},{"revisionInputs",revisionInputs},{"result",estimateJson(result)}};
    auto a=r["estimates"].toArray();a.append(snapshot);r["estimates"]=a;replaceCurrent(r);changed("Estimate snapshot issued",{},{{"id",snapshot["id"]}});refresh();
}
void MainWindow::compareRevisions(){
    auto revisions=project_["revisions"].toArray();if(revisions.size()<2)throw std::runtime_error("Import two revisions to compare");QStringList labels;for(auto v:revisions)labels<<v.toObject()["label"].toString();bool ok;
    const auto label=QInputDialog::getItem(this,"Compare","Compare current revision against:",labels,0,false,&ok);if(!ok)return;auto a=current(),b=revisions[labels.indexOf(label)].toObject();
    auto* t=table({"Measure",a["label"].toString(),b["label"].toString()});
    auto value=[](QJsonObject r,const char* key){return amount(r["geometry"].toObject()[key]);};row(t,{"Volume mm³",value(a,"volume"),value(b,"volume")});row(t,{"Surface mm²",value(a,"area"),value(b,"area")});
    for(auto k:{"features","findings","operations","issues"})row(t,{QString(k),QString::number(a[k].toArray().size()),QString::number(b[k].toArray().size())});
    auto time=[](QJsonObject r){double total=0;for(auto v:r["operations"].toArray()){auto o=operationFromJson(v.toObject());if(!o.timeKnown)return QString("UNKNOWN");total+=o.minutes;}return r["operations"].toArray().isEmpty()?QString("UNKNOWN"):QString::number(total,'f',3);};row(t,{"Cycle min/part",time(a),time(b)});
    auto cost=[](QJsonObject r){auto e=r["estimates"].toArray();if(e.isEmpty())return QString("No issued snapshot");auto s=e.last().toObject();return s["currency"].toString()+" "+amount(s["result"].toObject()["knownUnitTotal"])+" ("+amount(s["result"].toObject()["completeness"])+"%)";};row(t,{"Latest issued unit known subtotal",cost(a),cost(b)});
    QDialog d(this);d.setWindowTitle("Revision comparison — aggregate only");d.resize(950,480);auto* l=new QVBoxLayout(&d);auto* note=new QLabel("Counts include surface records. Issued costs may use different quantities, dates, currencies and rates; inspect snapshots before drawing conclusions. No automatic face correspondence.");note->setWordWrap(true);l->addWidget(note);l->addWidget(t);t->resizeColumnsToContents();d.exec();
}
void MainWindow::exportReport(bool pdf){
    const auto html=reportHtml(project_,current());auto path=QFileDialog::getSaveFileName(this,"Export report",pdf?"engineering-report.pdf":"engineering-report.html",pdf?"PDF (*.pdf)":"HTML (*.html)");if(path.isEmpty())return;
    if(pdf){QTextDocument doc;doc.setHtml(html);QPrinter printer(QPrinter::HighResolution);printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(path);doc.print(&printer);if(!QFileInfo::exists(path)||QFileInfo(path).size()==0)throw std::runtime_error("PDF export failed");}
    else {QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot write report");const auto bytes=html.toUtf8();if(f.write(bytes)!=bytes.size()||!f.commit())throw std::runtime_error("Report save failed");}statusBar()->showMessage("Report exported: "+path,5000);
}
void MainWindow::connectSql(){
    bool ok;auto text=QInputDialog::getText(this,"SQL Server (ODBC Driver 18)","ODBC connection string — not saved; integrated security recommended",QLineEdit::Password,
       "Driver={ODBC Driver 18 for SQL Server};Server=localhost;Database=CodeAsMetal;Trusted_Connection=Yes;Encrypt=Yes;TrustServerCertificate=No;",&ok);
    if(!ok||text.trimmed().isEmpty())return;sql_.connect(text);sqlVersion_.clear();statusBar()->showMessage("SQL Server connected. Open a server project to obtain its concurrency token.");
}
void MainWindow::openSql(){
    if(!sql_.connected())throw std::runtime_error("Connect to SQL Server first");if(!abandon())return;const auto projects=sql_.projects();if(projects.isEmpty())throw std::runtime_error("No server projects. Save a new local project to the server first.");QStringList labels;for(auto p:projects)labels<<p.second+" ["+p.first+"]";bool ok;auto item=QInputDialog::getItem(this,"Server projects","Project",labels,0,false,&ok);if(!ok)return;auto loaded=sql_.load(projects[labels.indexOf(item)].first);project_=loaded.first;sqlVersion_=loaded.second;localPath_.clear();revision_=project_["revisions"].toArray().isEmpty()?-1:0;dirty_=false;viewer_->clear();refresh();if(revision_>=0)showCad();
}
void MainWindow::saveSql(){if(!sql_.connected())throw std::runtime_error("Connect to SQL Server first");writeDocument(recoveryPath(),project_);auto token=sql_.save(project_,sqlVersion_);sqlVersion_=token;dirty_=false;setWindowModified(false);statusBar()->showMessage("Saved to SQL Server with optimistic concurrency; local recovery retained.",7000);}
}
