#include "cam/Document.h"
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QUuid>
#include <QSet>
#include <cmath>
#include <stdexcept>

namespace cam {
namespace {
QString q(const std::string& x){return QString::fromStdString(x);}
std::string s(const QJsonObject& o,const char* key){return o.value(key).toString().toStdString();}
double n(const QJsonObject& o,const char* key) {
    const auto v=o.value(key);
    if(!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()<0) throw std::invalid_argument(std::string("Invalid number: ")+key);
    return v.toDouble();
}
std::optional<double> optional(const QJsonObject& o,const char* key) {
    if(o.value(key).isNull()||o.value(key).isUndefined()) return {};return n(o,key);
}
QJsonValue j(std::optional<double> v){return v?QJsonValue(*v):QJsonValue(QJsonValue::Null);}
QJsonArray vectorJson(const std::array<double,3>& v){return {v[0],v[1],v[2]};}
}
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
QString nowUtc(){return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);}
QJsonObject newProject(const QString& name) {
    return {{"schema",1},{"id",uuid()},{"name",name},{"createdUtc",nowUtc()},
      {"currency","USD"},{"asOf",QDate::currentDate().toString(Qt::ISODate)},
      {"quantity",1},{"material",""},{"labor","operator"},{"inspector","quality"},
      {"stockMassKg",QJsonValue::Null},{"toolingPerPart",QJsonValue::Null},
      {"logisticsPerBatch",QJsonValue::Null},{"overheadPercent",QJsonValue::Null},
      {"rates",QJsonArray{}},{"rules",toJson(Rules{})},{"revisions",QJsonArray{}},{"audit",QJsonArray{}}};
}
QJsonObject toJson(const Feature& f) {
    QJsonArray faces;for(int x:f.faces)faces.append(x);
    return {{"id",q(f.id)},{"kind",q(f.kind)},{"evidence",q(f.evidence)},{"faces",faces},
      {"diameter",f.diameter},{"depth",f.depth},{"width",f.width},{"radius",f.radius},
      {"direction",vectorJson(f.direction)},{"confirmed",f.confirmed}};
}
Feature featureFromJson(const QJsonObject& o) {
    Feature f;f.id=s(o,"id");f.kind=s(o,"kind");f.evidence=s(o,"evidence");
    for(const auto& x:o["faces"].toArray()) {if(!x.isDouble()||x.toInt()<1||x.toDouble()!=x.toInt())throw std::invalid_argument("Invalid face reference");f.faces.push_back(x.toInt());}
    f.diameter=n(o,"diameter");f.depth=n(o,"depth");f.width=n(o,"width");f.radius=n(o,"radius");
    const auto direction=o["direction"].toArray();if(direction.size()!=3)throw std::invalid_argument("Invalid direction");
    for(int i=0;i<3;++i){if(!direction[i].isDouble()||!std::isfinite(direction[i].toDouble()))throw std::invalid_argument("Invalid axis");f.direction[i]=direction[i].toDouble();}
    if(!o["confirmed"].isBool())throw std::invalid_argument("Invalid feature confirmation");
    f.confirmed=o["confirmed"].toBool();return f;
}
QJsonObject toJson(const Operation& o) {
    return {{"id",q(o.id)},{"kind",q(o.kind)},{"setup",q(o.setup)},{"machine",q(o.machine)},{"tool",q(o.tool)},
      {"featureIds",q(o.featureIds)},{"basis",q(o.basis)},{"recommendedMinutes",j(o.recommendedMinutes)},{"minutes",o.minutes},
      {"timeKnown",o.timeKnown},{"setupMinutes",o.setupMinutes},{"setupKnown",o.setupKnown},
      {"overrideReason",q(o.overrideReason)},{"actor",q(o.actor)},{"changedUtc",q(o.changedUtc)}};
}
Operation operationFromJson(const QJsonObject& o) {
    Operation x;x.id=s(o,"id");x.kind=s(o,"kind");x.setup=s(o,"setup");x.machine=s(o,"machine");x.tool=s(o,"tool");
    x.featureIds=s(o,"featureIds");x.basis=s(o,"basis");x.recommendedMinutes=optional(o,"recommendedMinutes");x.minutes=n(o,"minutes");
    x.setupMinutes=n(o,"setupMinutes");
    if(!o["timeKnown"].isBool()||!o["setupKnown"].isBool())throw std::invalid_argument("Invalid time provenance");
    x.timeKnown=o["timeKnown"].toBool();x.setupKnown=o["setupKnown"].toBool();
    x.overrideReason=s(o,"overrideReason");x.actor=s(o,"actor");x.changedUtc=s(o,"changedUtc");return x;
}
QJsonObject toJson(const Rate& r){return {{"id",q(r.id)},{"key",q(r.key)},{"currency",q(r.currency)},
 {"from",q(r.from)},{"to",q(r.to)},{"source",q(r.source)},{"value",r.value}};}
Rate rateFromJson(const QJsonObject& o){return {s(o,"id"),s(o,"key"),s(o,"currency"),s(o,"from"),s(o,"to"),s(o,"source"),n(o,"value")};}
QJsonObject toJson(const Geometry& g){return {{"size",vectorJson(g.size)},{"center",vectorJson(g.center)},
 {"area",g.area},{"volume",j(g.volume)},{"solids",g.solids},{"shells",g.shells},{"faces",g.faces},{"edges",g.edges},{"vertices",g.vertices},{"mesh",g.mesh},{"valid",g.valid}};}
Rules rulesFromJson(const QJsonObject& o){Rules r{s(o,"version"),n(o,"maxHoleRatio"),n(o,"minWall"),n(o,"minRadius")};validate(r);return r;}
QJsonObject toJson(const Rules& r){return {{"version",q(r.version)},{"maxHoleRatio",r.maxHoleRatio},{"minWall",r.minWall},{"minRadius",r.minRadius}};}
QJsonArray findingsJson(const std::vector<Finding>& f) {
    QJsonArray a;for(const auto& x:f)a.append(QJsonObject{{"feature",q(x.featureId)},{"rule",q(x.rule)},{"version",q(x.version)},
      {"severity",q(x.severity)},{"explanation",q(x.explanation)},{"measured",x.measured},{"threshold",x.threshold}});return a;
}
CostInput costInput(const QJsonObject& p,const QJsonObject& r) {
    CostInput in;const double qty=n(p,"quantity");
    if(qty<1||qty>100000000||qty!=std::floor(qty))throw std::invalid_argument("Invalid integer quantity");
    in.quantity=static_cast<int>(qty);in.currency=s(p,"currency");in.asOf=s(p,"asOf");in.material=s(p,"material");in.labor=s(p,"labor");in.inspector=s(p,"inspector");
    in.stockMassKg=optional(p,"stockMassKg");in.toolingPerPart=optional(p,"toolingPerPart");in.logisticsPerBatch=optional(p,"logisticsPerBatch");in.overheadPercent=optional(p,"overheadPercent");
    for(const auto& x:p["rates"].toArray())in.rates.push_back(rateFromJson(x.toObject()));
    for(const auto& x:r["operations"].toArray())in.operations.push_back(operationFromJson(x.toObject()));return in;
}
QJsonObject estimateJson(const Estimate& e) {
    QJsonArray a;for(const auto& l:e.lines)a.append(QJsonObject{{"category",q(l.category)},{"amount",j(l.amount)},{"basis",q(l.basis)}});
    return {{"lines",a},{"knownBatchTotal",e.knownBatchTotal},{"knownUnitTotal",e.knownUnitTotal},
      {"completeness",e.completeness},{"complete",e.complete},{"modelVersion",q(e.modelVersion)}};
}
void validateDocument(const QJsonObject& p) {
    if(p["schema"].toInt()!=1||QUuid(p["id"].toString()).isNull()||p["name"].toString().trimmed().isEmpty())throw std::invalid_argument("Unsupported or malformed CodeAsMetal project");
    if(!p["rates"].isArray()||!p["revisions"].isArray()||!p["audit"].isArray())throw std::invalid_argument("Missing project collections");
    rulesFromJson(p["rules"].toObject());estimate(costInput(p,{}));QSet<QString> ids,labels;
    for(const auto& v:p["revisions"].toArray()) {
        const auto r=v.toObject();const auto id=r["id"].toString(),label=r["label"].toString();
        if(QUuid(id).isNull()||ids.contains(id)||label.isEmpty()||labels.contains(label))throw std::invalid_argument("Invalid/duplicate revision identity");ids.insert(id);labels.insert(label);
        if(r["sha256"].toString().size()!=64||!r["features"].isArray()||!r["operations"].isArray()||!r["issues"].isArray()||!r["estimates"].isArray())throw std::invalid_argument("Malformed revision");
        QSet<QString> featureIds;
        for(const auto& f:r["features"].toArray()) {const auto x=featureFromJson(f.toObject());const auto key=q(x.id);if(key.isEmpty()||featureIds.contains(key))throw std::invalid_argument("Duplicate feature ID");featureIds.insert(key);}
        estimate(costInput(p,r));
    }
}
void writeDocument(const QString& path,const QJsonObject& p) {
    validateDocument(p);QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot open project for writing");
    const auto bytes=QJsonDocument(p).toJson(QJsonDocument::Indented);
    if(f.write(bytes)!=bytes.size()||!f.commit())throw std::runtime_error("Atomic project save failed");
}
QJsonObject readDocument(const QString& path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>100*1024*1024)throw std::runtime_error("Cannot open project or project exceeds 100 MiB");
    QJsonParseError e;auto doc=QJsonDocument::fromJson(f.readAll(),&e);
    if(e.error!=QJsonParseError::NoError||!doc.isObject())throw std::runtime_error("Malformed JSON project");
    validateDocument(doc.object());return doc.object();
}
QString reportHtml(const QJsonObject& p,const QJsonObject& r) {
    const auto e=estimate(costInput(p,r));auto esc=[](QString x){return x.toHtmlEscaped();};
    QString h="<!doctype html><meta charset='utf-8'><title>CodeAsMetal Engineering Report</title><style>body{font:15px Arial;max-width:1100px;margin:40px auto;color:#182b3a}table{border-collapse:collapse;width:100%}td,th{border:1px solid #bbc7d0;padding:8px;text-align:left}pre{white-space:pre-wrap;font-size:12px}.warning{color:#975309}</style>";
    h+="<h1>CodeAsMetal · Engineering review</h1><h2>"+esc(p["name"].toString())+" / "+esc(r["label"].toString())+"</h2>";
    h+="<p>Generated UTC: "+nowUtc()+"<br>CAD SHA-256: "+esc(r["sha256"].toString())+"<br>Engine: cad-1.0 / cost-1.0</p>";
    h+="<p class='warning'>Preliminary engineering estimate. Requires manufacturing review. Completeness measures supplied inputs; it does not establish accuracy, confidence or manufacturability.</p>";
    h+="<h2>Current estimate · "+QString(e.complete?"Complete inputs":"Partial inputs")+"</h2><p>Currency: "+esc(p["currency"].toString())+" · Quantity: "+QString::number(p["quantity"].toInt())+" · Completeness: "+QString::number(e.completeness,'f',1)+"%</p><table><tr><th>Batch category</th><th>Amount</th><th>Basis</th></tr>";
    for(const auto& l:e.lines)h+="<tr><td>"+esc(q(l.category))+"</td><td>"+(l.amount?QString::number(*l.amount,'f',2):"MISSING")+"</td><td>"+esc(q(l.basis))+"</td></tr>";
    h+="</table><p>Known subtotal / batch: "+QString::number(e.knownBatchTotal,'f',2)+" · per part: "+QString::number(e.knownUnitTotal,'f',2)+"</p>";
    for(const auto& key:{"geometry","features","findings","operations","issues","estimates"}) {
        const QJsonValue v=r[key];QByteArray bytes=v.isArray()?QJsonDocument(v.toArray()).toJson():QJsonDocument(v.toObject()).toJson();
        h+="<h2>"+QString(key)+"</h2><pre>"+esc(QString::fromUtf8(bytes))+"</pre>";
    }
    return h;
}
}
