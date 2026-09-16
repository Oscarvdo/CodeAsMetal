#pragma once
#include <QWidget>
#include <QPoint>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <TopoDS_Shape.hxx>
#include <functional>
namespace cam {
/** Native OCCT rendering surface. All methods execute on the UI thread.
 * Left drag=orbit, middle/right drag=pan, wheel=zoom, click=select. */
class Viewer final : public QWidget {
public:
    explicit Viewer(QWidget* parent=nullptr);
    ~Viewer() override;
    void display(const TopoDS_Shape&);
    void clear();
    void fit();
    void standardView(int index);
    void selectionMode(int dimension);
    void highlightFace(int oneBasedIndex);
    std::function<void(int)> faceSelected;
protected:
    QPaintEngine* paintEngine()const override{return failed_?QWidget::paintEngine():nullptr;}
    void paintEvent(QPaintEvent*)override;
    void resizeEvent(QResizeEvent*)override;
    void mousePressEvent(QMouseEvent*)override;
    void mouseMoveEvent(QMouseEvent*)override;
    void mouseReleaseEvent(QMouseEvent*)override;
    void wheelEvent(QWheelEvent*)override;
private:
    void initialize();
    QPoint physical(const QPointF&) const;
    Handle(AIS_InteractiveContext) context_;
    Handle(V3d_View) view_;
    Handle(AIS_Shape) object_;
    TopoDS_Shape shape_;
    QPoint last_,pressed_;
    bool failed_{};
};
}
