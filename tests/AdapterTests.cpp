/** CAD/document integration acceptance tests. Require Qt/OCCT and Windows;
 * generated fixtures contain exact analytic dimensions, no downloaded CAD. */
#include "cam/Cad.h"
#include "cam/Document.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <gtest/gtest.h>
#include <numbers>
TEST(Cad,BlockHasExpectedEngineeringMeasures){
    QTemporaryDir d;ASSERT_TRUE(d.isValid());cam::generateSamples(d.path());auto m=cam::importCad(d.path()+"/block.step");
    ASSERT_TRUE(m->geometry.volume);EXPECT_NEAR(*m->geometry.volume,120000,1e-4);
    EXPECT_NEAR(m->geometry.area,18400,1e-4);EXPECT_EQ(m->geometry.faces,6);EXPECT_EQ(m->geometry.solids,1);
    EXPECT_NEAR(m->geometry.center[0],50,1e-6);EXPECT_NEAR(m->geometry.center[1],30,1e-6);EXPECT_NEAR(m->geometry.center[2],10,1e-6);
}
TEST(Cad,HolesRemoveExactVolumeAndRemainCandidates){
    QTemporaryDir d;cam::generateSamples(d.path());auto m=cam::importCad(d.path()+"/holes.step");ASSERT_TRUE(m->geometry.volume);
    EXPECT_NEAR(*m->geometry.volume,120000-std::numbers::pi*(9*20+25*12),1e-3);
    int holes=0;for(auto& f:m->features)if(f.kind=="Hole candidate"){++holes;EXPECT_FALSE(f.confirmed);}EXPECT_EQ(holes,2);
}
TEST(Cad,StlNeverClaimsExactSolidVolumeOrFeatures){
    QTemporaryDir d;cam::generateSamples(d.path());auto m=cam::importCad(d.path()+"/holes-mm.stl");
    EXPECT_TRUE(m->geometry.mesh);EXPECT_FALSE(m->geometry.volume);EXPECT_TRUE(m->features.empty());
    auto inches=cam::importCad(d.path()+"/holes-mm.stl",25.4);EXPECT_NEAR(inches->geometry.size[0]/m->geometry.size[0],25.4,1e-5);
}
TEST(Cad,MalformedStepIsRejected){QTemporaryDir d;QFile f(d.path()+"/bad.step");ASSERT_TRUE(f.open(QIODevice::WriteOnly));f.write("not STEP");f.close();EXPECT_THROW(cam::importCad(f.fileName()),std::runtime_error);}
TEST(Document,AtomicSaveRoundTripAndTamperDetection){
    QTemporaryDir d;auto p=cam::newProject("Test");cam::writeDocument(d.path()+"/p.cam.json",p);EXPECT_EQ(cam::readDocument(d.path()+"/p.cam.json"),p);
    auto invalid=p;invalid["quantity"]="not numeric";EXPECT_THROW(cam::writeDocument(d.path()+"/p.cam.json",invalid),std::invalid_argument);EXPECT_EQ(cam::readDocument(d.path()+"/p.cam.json"),p);
}
TEST(Document,HtmlEscapesUserContent){
    auto p=cam::newProject("<script>alert(1)</script>");QJsonObject r{{"label","A"},{"sha256","test"},{"operations",QJsonArray{}}};
    auto h=cam::reportHtml(p,r);EXPECT_FALSE(h.contains("<script>"));EXPECT_TRUE(h.contains("&lt;script&gt;"));
}
TEST(Document,FileHashChangesWhenBytesChange){
    QTemporaryDir d;QFile f(d.path()+"/bytes.bin");ASSERT_TRUE(f.open(QIODevice::WriteOnly));f.write("A");f.close();auto h=cam::hashFile(f.fileName());
    ASSERT_TRUE(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write("B");f.close();EXPECT_NE(cam::hashFile(f.fileName()),h);
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}
