#pragma once
#include "cam/Domain.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QByteArray>

namespace cam {
/** Versioned application document. JSON is an adapter format, not domain logic.
 * Issued estimates embed all inputs. Opening never recalculates old estimates. */
QJsonObject newProject(const QString& name);
QString uuid();
QString nowUtc();
QJsonObject toJson(const Feature&);
Feature featureFromJson(const QJsonObject&);
QJsonObject toJson(const Operation&);
Operation operationFromJson(const QJsonObject&);
QJsonObject toJson(const Rate&);
Rate rateFromJson(const QJsonObject&);
QJsonObject toJson(const Geometry&);
Rules rulesFromJson(const QJsonObject&);
QJsonObject toJson(const Rules&);
QJsonArray findingsJson(const std::vector<Finding>&);
CostInput costInput(const QJsonObject& project,const QJsonObject& revision);
QJsonObject estimateJson(const Estimate&);
/** Strict structural/numeric validation of externally loaded documents. */
void validateDocument(const QJsonObject&);
/** Atomic local save. Recovery files use the same versioned format. */
void writeDocument(const QString&,const QJsonObject&);
QJsonObject readDocument(const QString&);
/** Escape all user text before HTML output. Report uses stored/explicit inputs. */
QString reportHtml(const QJsonObject&,const QJsonObject&);
}
