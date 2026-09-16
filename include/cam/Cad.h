#pragma once
#include "cam/Domain.h"
#include <TopoDS_Shape.hxx>
#include <QString>
#include <memory>

namespace cam {
/** Immutable import result. Native shape ownership remains within CAD/UI adapters.
 * Import is serialized because STEP translator configuration is process-global. */
struct CadModel {
    TopoDS_Shape shape;
    Geometry geometry;
    std::vector<Feature> features;
    QString sourcePath, sha256, warning, kernelVersion;
};
/** Read STEP in mm, or STL with explicit input-units scale. Reject null data,
 * failed transfer and changing source bytes. Does not silently repair solids. */
std::shared_ptr<CadModel> importCad(const QString& path, double stlScaleToMm=1);
/** SHA-256 of actual bytes; throws on I/O errors. */
QString hashFile(const QString& path);
/** Generate real analytic regression/demo solids and STEP/STL files in directory. */
void generateSamples(const QString& directory);
}
