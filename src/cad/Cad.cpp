#include "cam/Cad.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Trsf.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <cmath>
#include <mutex>
#include <numbers>
#include <stdexcept>

namespace cam {
namespace {
std::mutex translatorMutex;
int count(const TopoDS_Shape& s,TopAbs_ShapeEnum type) { TopTools_IndexedMapOfShape m;TopExp::MapShapes(s,type,m);return m.Extent(); }
// Require empty axis and material just beyond the cylindrical surface. This
// rejects external bosses. Partial cylinders remain surface records only.
bool cavity(const TopoDS_Shape& solid,const gp_Cylinder& cylinder,double z,double r) {
    auto center=cylinder.Location().Translated(gp_Vec(cylinder.Axis().Direction())*z);
    BRepClass3d_SolidClassifier axis(solid,center,1e-6);
    const auto radial=gp_Vec(cylinder.Position().XDirection());
    BRepClass3d_SolidClassifier wall(solid,center.Translated(radial*(r+std::max(1e-4,r*0.01))),1e-6);
    return axis.State()==TopAbs_OUT && wall.State()==TopAbs_IN;
}
}
QString hashFile(const QString& path) {
    QFile f(path); if(!f.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read source file");
    QCryptographicHash h(QCryptographicHash::Sha256);
    if(!h.addData(&f)) throw std::runtime_error("Failed while hashing source file");
    return QString::fromLatin1(h.result().toHex());
}
std::shared_ptr<CadModel> importCad(const QString& path,double scale) {
    std::lock_guard lock(translatorMutex);
    try {
        if(!std::isfinite(scale)||scale<=0) throw std::invalid_argument("STL scale must be positive");
        auto m=std::make_shared<CadModel>(); m->kernelVersion=OCC_VERSION_COMPLETE; m->sourcePath=QFileInfo(path).absoluteFilePath(); m->sha256=hashFile(path);
        const auto ext=QFileInfo(path).suffix().toLower();
        if(ext!="step"&&ext!="stp"&&ext!="stl") throw std::invalid_argument("Use STEP, STP or STL");
        // Stage a stable byte snapshot before transfer. OCCT file APIs consume
        // UTF-8 paths; the original source is re-hashed after analysis.
        QTemporaryDir temporary(QDir::tempPath()+"/cam-XXXXXX");
        if(!temporary.isValid()) throw std::runtime_error("Cannot allocate CAD staging directory");
        const QString staged=temporary.path()+"/source."+ext;
        if(!QFile::copy(path,staged)) throw std::runtime_error("Cannot stage CAD bytes");
        if(hashFile(staged)!=m->sha256) throw std::runtime_error("Source changed while copying");
        const auto bytes=staged.toUtf8();
        if(ext=="stl") {
            m->geometry.mesh=true;
            StlAPI_Reader reader;
            if(!reader.Read(m->shape,bytes.constData())) throw std::runtime_error("STL import failed");
            if(scale!=1) {gp_Trsf tr;tr.SetScale(gp_Pnt(0,0,0),scale);m->shape=BRepBuilderAPI_Transform(m->shape,tr,true).Shape();}
            m->warning="STL: triangle mesh only. Explicit units; no exact volume, B-Rep features or automatic DFM.";
        } else {
            STEPControl_Reader reader;
            Interface_Static::SetCVal("xstep.cascade.unit","MM");
            if(reader.ReadFile(bytes.constData())!=IFSelect_RetDone || reader.TransferRoots()<1)
                throw std::runtime_error("STEP parse/transfer failed");
            m->shape=reader.OneShape();
        }
        if(m->shape.IsNull()) throw std::runtime_error("Empty CAD shape");
        auto& g=m->geometry;
        g.solids=count(m->shape,TopAbs_SOLID);g.shells=count(m->shape,TopAbs_SHELL);
        g.faces=count(m->shape,TopAbs_FACE);g.edges=count(m->shape,TopAbs_EDGE);g.vertices=count(m->shape,TopAbs_VERTEX);
        if(!g.faces) throw std::runtime_error("CAD contains no faces");
        g.valid=BRepCheck_Analyzer(m->shape).IsValid();
        Bnd_Box box;BRepBndLib::Add(m->shape,box);box.SetGap(0);
        if(box.IsVoid()||box.IsOpen()) throw std::runtime_error("Unbounded geometry");
        double x0,y0,z0,x1,y1,z1;box.Get(x0,y0,z0,x1,y1,z1);
        g.size={x1-x0,y1-y0,z1-z0};
        GProp_GProps props;BRepGProp::SurfaceProperties(m->shape,props);g.area=props.Mass();
        g.center={props.CentreOfMass().X(),props.CentreOfMass().Y(),props.CentreOfMass().Z()};
        TopoDS_Shape solid;
        if(!g.mesh && g.valid && g.solids==1) {
            for(TopExp_Explorer it(m->shape,TopAbs_SOLID);it.More();it.Next()) solid=it.Current();
            // Reject additional loose shells/faces: OneShape may contain mixed
            // contents, so a single solid count alone does not establish truth.
            if(count(solid,TopAbs_FACE)==g.faces) {
                BRepGProp::VolumeProperties(solid,props,true);
                if(props.Mass()>0) {g.volume=props.Mass();g.center={props.CentreOfMass().X(),props.CentreOfMass().Y(),props.CentreOfMass().Z()};}
            }
        }
        if(!g.mesh && !g.volume) m->warning="Review: only one valid closed solid supports volume and cavity screening. Assemblies/open shells remain visualizable.";
        if(!g.mesh) {
            TopTools_IndexedMapOfShape faces;TopExp::MapShapes(m->shape,TopAbs_FACE,faces);
            for(int i=1;i<=faces.Extent();++i) {
                auto face=TopoDS::Face(faces(i));BRepAdaptor_Surface surface(face,true);
                Feature f;f.id="F"+std::to_string(i);f.faces={i};
                f.evidence="OCCT B-Rep surface; identity scoped to SHA-256 and cad-1.0; not transferable between revisions.";
                if(surface.GetType()==GeomAbs_Plane) {f.kind="Planar face";f.confirmed=true;}
                else if(surface.GetType()==GeomAbs_Cylinder) {
                    const auto c=surface.Cylinder();
                    double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
                    f.kind="Cylindrical surface";f.diameter=2*c.Radius();f.depth=std::abs(v1-v0);
                    f.direction={c.Axis().Direction().X(),c.Axis().Direction().Y(),c.Axis().Direction().Z()};
                    if(g.volume && std::isfinite(f.depth) && std::abs((u1-u0)-2*std::numbers::pi)<1e-5 &&
                       cavity(solid,c,(v0+v1)/2,c.Radius())) {
                        f.kind="Hole candidate";
                        f.evidence+=" Full cylinder, empty center and exterior radial material at midpoint. End conditions and intersecting features must be confirmed by engineer.";
                    }
                } else if(surface.GetType()==GeomAbs_Cone) f.kind="Conical surface";
                else if(surface.GetType()==GeomAbs_Torus) f.kind="Toroidal surface";
                else f.kind="Other surface";
                m->features.push_back(std::move(f));
            }
        }
        if(hashFile(path)!=m->sha256) throw std::runtime_error("Source changed during analysis; import again");
        return m;
    } catch(const Standard_Failure& e) {throw std::runtime_error(e.GetMessageString()?e.GetMessageString():"OCCT failure");}
}
void generateSamples(const QString& directory) {
    std::lock_guard lock(translatorMutex);QDir().mkpath(directory);
    auto box=BRepPrimAPI_MakeBox(100,60,20).Shape();
    auto through=BRepAlgoAPI_Cut(box,BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(25,30,-1),gp_Dir(0,0,1)),3,22).Shape()).Shape();
    auto blind=BRepAlgoAPI_Cut(through,BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(70,30,8),gp_Dir(0,0,1)),5,13).Shape()).Shape();
    const std::pair<const char*,TopoDS_Shape> samples[]={{"block.step",box},{"holes.step",blind}};
    for(const auto& [name,shape]:samples) {
        STEPControl_Writer writer; if(writer.Transfer(shape,STEPControl_AsIs)!=IFSelect_RetDone ||
          writer.Write((directory+"/"+name).toUtf8().constData())!=IFSelect_RetDone)
            throw std::runtime_error("Cannot write demo STEP");
    }
    BRepMesh_IncrementalMesh mesh(blind,0.1);StlAPI_Writer writer;
    if(!writer.Write(blind,(directory+"/holes-mm.stl").toUtf8().constData())) throw std::runtime_error("Cannot write demo STL");
}
}
