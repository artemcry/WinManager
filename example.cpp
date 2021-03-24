#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QBoxLayout>
#include <QDebug>
#include <QCheckBox>
#include "winmanager.h"
#define cc qDebug()

static QColor average_screen_color;
// This variable stores the average of the inverted screen color.
// Value only needs to be updated when pressed as the function is slow

// This function updates it
void update_average_screen_color()
{
    QImage img = qApp->primaryScreen()->grabWindow(static_cast<WId>(qApp->desktop()->screenNumber())).toImage();
    int r=0,g=0,b=0,rate = 20,w = img.width(),h = img.height(),itc = w*h/(rate*rate);

    for(int i = 0; i < w; i+= rate)
        for(int j = 0; j < h; j+= rate) {
            r += img.pixelColor(i,j).red();
            g += img.pixelColor(i,j).green();
            b += img.pixelColor(i,j).blue();
        }
    average_screen_color = QColor(abs((r/itc)-255),abs((g/itc)-255),abs((b/itc)-255));
}

void resize_painter(const QWidget *win,QPainter &p)
{
    QPen pn;
    int w = 3;
    pn.setWidth(w);
    pn.setColor(average_screen_color);
    pn.setJoinStyle(Qt::MiterJoin);
    p.setPen(pn);
    p.drawRect(2,2,win->width()-w-1,win->height()-w-1);
}

void snapPaintFunc(const QWidget *win,QPainter &p)
{
    p.setPen(QPen(Qt::NoPen));
    p.setBrush(QColor(100, 100, 230,70));
    p.drawRect(0,0,win->width(),win->height());
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QWidget w;
    w.setGeometry(0,0,50,50);
    w.setMinimumSize(QSize(200,200));
    w.setStyleSheet("QWidget{background-color: qlineargradient(spread:pad, x1:0, y1:1, x2:1, y2:0, stop:0 rgba(170, 108, 0, 255), stop:1 rgba(91, 77, 174, 255))}");

//-------------------------------------------------
    WinManager *p = new WinManager(&w);
    QObject::connect(p,&WinManager::resizeFrameClicked,&a,[](){ update_average_screen_color(); });


    //p->overrideSideSnapSides(WM::right|WM::left|WM::bottom_right);

    p->setBorderWidth(10);
    p->setMoveCursor(Qt::ClosedHandCursor);
    p->setResizePaintFunction(::resize_painter);
    p->setSnapPaintFunction(::snapPaintFunc);
    p->overrideFlags(WM::Flag::SaveGeometry|WM::HalfSnap);
    //p->disableFlags(WM::HalfSnap);

//-------------------------------------------------

    QPushButton *min = new QPushButton("min",&w);
    QPushButton *max = new QPushButton("max",&w);
    QPushButton *qui = new QPushButton("quit",&w);
    QCheckBox *ch = new QCheckBox("Show frame",&w);
    QCheckBox *ch2 = new QCheckBox("Show rect",&w);

    QObject::connect(ch,&QCheckBox::stateChanged,&w,[&](int s){
        if (s == 0)
            p->showResizeFrame(QColor(0,0,0,0));
        else
            p->showResizeFrame(QColor(150,150,150,100));
    });

    QObject::connect(ch2,&QCheckBox::stateChanged,&w,[&](int s){
        if (s == 0)
            p->disableFlags(WM::DrawResizeRect);
        else
            p->setFlags(WM::DrawResizeRect);
    });
    ch->setChecked(true);
    ch2->setChecked(true);
    QVBoxLayout *vlayout= new QVBoxLayout(&w);
    QHBoxLayout *hlayout = new QHBoxLayout(&w);
    QSpacerItem *vspacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QSpacerItem *hspacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    p->setMinimizeButton(min);
    p->setMaximizeButton(max);
    p->setQuitButton(qui);
    min->setMinimumHeight(25);
    max->setMinimumHeight(25);
    qui->setMinimumHeight(25);
    min->setMaximumWidth(50);
    max->setMaximumWidth(50);
    qui->setMaximumWidth(50);
    ch->setMaximumWidth(80);
    ch2->setMaximumWidth(80);
    vlayout->setContentsMargins(5,5,5,5);
    hlayout->setSpacing(0);
    vlayout->addLayout(hlayout);
    hlayout->addSpacerItem(hspacer);
    hlayout->addWidget(ch,1);
    hlayout->addWidget(ch2,1);
    hlayout->addWidget(min,1);
    hlayout->addWidget(max,1);
    hlayout->addWidget(qui,1);
    vlayout->addSpacerItem(vspacer);
    min->setStyleSheet("QPushButton { font: 14 bold; color: rgba(220,220,220,220); border: none;}"
                       "QPushButton { background: rgba(90,90,90,140);}"
                       "QPushButton:hover  { background: rgba(150,150,150,140);}");

    qui->setStyleSheet("QPushButton { font: 14 bold; color: rgba(220,220,220,220) ; border: none;}"
                       "QPushButton { background: rgba(90,90,90,140);}"
                       "QPushButton:hover  { background: rgba(150,150,150,140);}");
    p->setMaximizeButtonProperty("test");
    max->setStyleSheet(
                "QPushButton { font: 14 bold; color: rgba(220,220,220,220) ; border: none;}"
                "QPushButton[test=\"false\"] { background: rgba(0,200,0,140);}"
                "QPushButton[test=\"false\"]:hover  { background: rgba(0,255,0,140);}"
                "QPushButton[test=\"true\"] { background: rgba(200,0,0,140);}"
                "QPushButton[test=\"true\"]:hover { background: rgba(255,0,0,140);}");
    w.show();
    return a.exec();
}
