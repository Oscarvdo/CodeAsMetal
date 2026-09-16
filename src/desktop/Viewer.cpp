#include "cam/Viewer.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QMessageBox>
#include <QTimer>
#include <Standard_Failure.hxx>
#include <AIS_DisplayMode.hxx>
#include <AIS_SelectionScheme.hxx>
#include <stdexcept>
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_Viewer.hxx>
#include <WNT_Window.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <Quantity_Color.hxx>
#include <Prs3d_Drawer.hxx>
#include <TopoDS.hxx>
namespace cam {
Viewer::Viewer(QWidget* p):QWidget(p) {
    setAttribute(Qt::WA_NativeWindow);setAttribute(Qt::WA_PaintOnScreen);setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);setMinimumSize(380,300);
}
Viewer::~Viewer(){if(!view_.IsNull())view_->Remove();}
QPoint Viewer::physical(const QPointF& p)const{return {(int)(p.x()*devicePixelRatioF()),(int)(p.y()*devicePixelRatioF())};}
void Viewer::initialize(){
    if(failed_)throw std::runtime_error("OpenGL viewer initialization failed. Restart after correcting the graphics driver.");
    if(!view_.IsNull())return;
    Handle(Aspect_DisplayConnection) display=new Aspect_DisplayConnection;
    Handle(OpenGl_GraphicDriver) driver=new OpenGl_GraphicDriver(display);
    Handle(V3d_Viewer) viewer=new V3d_Viewer(driver);viewer->SetDefaultLights();viewer->SetLightOn();
    context_=new AIS_InteractiveContext(viewer);view_=viewer->CreateView();
    Handle(WNT_Window) window=new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
    view_->SetWindow(window);if(!window->IsMapped())window->Map();
    view_->SetBackgroundColor(Quantity_Color(0.075,0.11,0.16,Quantity_TOC_RGB));
    view_->TriedronDisplay(Aspect_TOTP_LEFT_LOWER,Quantity_NOC_WHITE,0.08,V3d_ZBUFFER);view_->MustBeResized();
}
void Viewer::display(const TopoDS_Shape& shape){
    initialize();context_->RemoveAll(false);shape_=shape;object_=new AIS_Shape(shape);
    object_->SetColor(Quantity_Color(0.69,0.76,0.81,Quantity_TOC_RGB));
    context_->Display(object_,AIS_Shaded,0,false);selectionMode(2);view_->SetProj(V3d_XposYnegZpos);fit();
}
void Viewer::clear(){shape_.Nullify();object_.Nullify();if(!context_.IsNull())context_->RemoveAll(true);}
void Viewer::fit(){if(!view_.IsNull()){view_->FitAll();view_->ZFitAll();view_->Redraw();}}
void Viewer::standardView(int i){initialize();const V3d_TypeOfOrientation views[]={V3d_XposYnegZpos,V3d_Zpos,V3d_Yneg,V3d_Xpos};view_->SetProj(views[i%4]);fit();}
void Viewer::selectionMode(int d){if(object_.IsNull())return;context_->Deactivate(object_);context_->Activate(object_,AIS_Shape::SelectionMode(d==1?TopAbs_EDGE:d==2?TopAbs_FACE:TopAbs_SOLID));}
void Viewer::highlightFace(int index){
    if(object_.IsNull())return;TopTools_IndexedMapOfShape faces;TopExp::MapShapes(shape_,TopAbs_FACE,faces);
    if(index<1||index>faces.Extent())return;
    context_->ClearSelected(false);
    Handle(StdSelect_BRepOwner) owner=new StdSelect_BRepOwner(faces(index),object_,0,true);
    context_->AddOrRemoveSelected(owner,true);
}
void Viewer::paintEvent(QPaintEvent*){
    if(failed_){QPainter painter(this);painter.fillRect(rect(),Qt::darkGray);painter.setPen(Qt::white);painter.drawText(rect(),Qt::AlignCenter,"3D viewer unavailable — see graphics diagnostics");return;}
    try {initialize();view_->Redraw();}
    catch(const Standard_Failure& failure){
        failed_=true;view_.Nullify();context_.Nullify();
        setAttribute(Qt::WA_PaintOnScreen,false);setAttribute(Qt::WA_NoSystemBackground,false);
        const auto message=QString::fromUtf8(failure.GetMessageString()?failure.GetMessageString():"OpenGL initialization failed");
        QTimer::singleShot(0,this,[this,message]{QMessageBox::critical(this,"3D graphics",message);update();});
    }
}
void Viewer::resizeEvent(QResizeEvent*){if(!view_.IsNull()){view_->MustBeResized();view_->Redraw();}}
void Viewer::mousePressEvent(QMouseEvent* e){initialize();last_=pressed_=physical(e->position());if(e->button()==Qt::LeftButton)view_->StartRotation(last_.x(),last_.y());}
void Viewer::mouseMoveEvent(QMouseEvent* e){
    if(view_.IsNull())return;const auto p=physical(e->position());
    if(e->buttons()&Qt::LeftButton)view_->Rotation(p.x(),p.y());
    else if(e->buttons()&(Qt::MiddleButton|Qt::RightButton))view_->Pan(p.x()-last_.x(),last_.y()-p.y());
    else context_->MoveTo(p.x(),p.y(),view_,true);last_=p;
}
void Viewer::mouseReleaseEvent(QMouseEvent* e){
    if(context_.IsNull()||e->button()!=Qt::LeftButton)return;const auto p=physical(e->position());
    if((p-pressed_).manhattanLength()>5*devicePixelRatioF())return;
    context_->MoveTo(p.x(),p.y(),view_,false);context_->SelectDetected(AIS_SelectionScheme_Replace);context_->UpdateCurrentViewer();
    context_->InitSelected();if(context_->MoreSelected()&&faceSelected){TopTools_IndexedMapOfShape faces;TopExp::MapShapes(shape_,TopAbs_FACE,faces);faceSelected(faces.FindIndex(context_->SelectedShape()));}
}
void Viewer::wheelEvent(QWheelEvent* e){if(!view_.IsNull()){view_->SetScale(view_->Scale()*(e->angleDelta().y()>0?1.15:1/1.15));view_->Redraw();}}
}
