#include "winmanager.h"
#define cr QCursor::pos()
#define dsk QGuiApplication::primaryScreen()->availableGeometry()

#define S_GEOMETRY "__geometry"

#define cmd qDebug()
using namespace WM;

class WinManagerPrivate : public QObject
{
    class ResizeRect;
    friend class WinManager;

    WinManagerPrivate(WinManager *mainClass,QWidget *window);
    ~WinManagerPrivate();

    WinManager *manager;
    QWidget *window;
    QWidget *frame;
    ResizeRect *resizeRect;

    QCursor oldCursor; // Cursor before moving
    QCursor movingCursor;
    QRect oldWindowGeometry; // window geometry before maximizing
    QPoint ppos; // Window capture point
    Side pside; // This is for optimization

    QRect defaultGeometry;

    const char *maximizedButtonProperty; // Pseudo-state of maximize button for CSS
    QAbstractButton *maximizeButton;
    QAbstractButton *minimizeButton;
    QAbstractButton *quitButton;

    int movingArea;
    int borderWidth; // #frame width
    int maxX; // Maximum X | Y when changing the window size
    int maxY;
    Side captureSide; // The side where the window was captured
    QPoint offset; // Offset of the cursor relative to the edges of the window when the window is resizing


    Flags flags; // Flags for different settings
    Sides sideSnapSides; // Sides of desktop to which the window can be snap
    Sides maximizeSides; // Desktop sides that will maximize the window
    Side currentSnapSide; // Current snap to the desktop side


    PaintFunction paintFunc;
    PaintFunction resizePaintFunc; // Drawing function #rect
    PaintFunction snapPaintFunc;   // Drawing function #rect to snap to edge desktop

    bool f_moving;
    bool f_start;

    inline void clearSideSnap()
    { currentSnapSide = Side::none; }

    inline void adjustX(QPoint &pos)
    { if(pos.x() > maxX) pos.setX(maxX); }

    inline void adjustY(QPoint &pos)
    { if(pos.y() > maxY) pos.setY(maxY); }

    inline bool windowIsSnapped()
    { return  currentSnapSide != Side::none; }

    static QPoint getOffset(QWidget *window, Side side,const QPoint& pos);
    // Getting the offset of the point relative to the side of the window
    void resizeWindowToCursor(QWidget *window);   

    void loadWindowGeometry();
    void saveWindowGeometry();
    void moveWindow();
    void checkMouseRelease();
    void checkMousePress();
    void updateCursor(QWidget *window);
    // Changes the view of the cursor to match the side of the window in which it is located
    void updateFrameMask();
    // Changes the #frame to fit the size, state, and snap of the window
    void minimizeWindow();
    void maximizeWindow();
    void quitApp();
    void deleteResizeRect();
    void snapWindow(QWidget *window, Side side);

    static Side getDesktopSide(const QPoint &point);
    // Returns the side of desktop for given coordinates

    void adjustWinForDesktop();
    // Aligns the window to desktop

    Side getWindowSide(const QPoint &point) const;
    // Returns the side of the window for the given coordinates

    void desktopGeometryChanged(const QRect &);
    // Handles the desktop resize or position change event

    void adjustSnap(const QRect &rect = dsk);
    // Aligns the window to the desktop if window is maximized or snap to the side

    bool eventFilter(QObject *sender, QEvent *event);
    class ResizeRect:public QWidget
    {
    public:
        // For snap to the desktop side
        ResizeRect(WinManagerPrivate* parent,const QRect &rect);
        // For resize
        ResizeRect(WinManagerPrivate* parent,QWidget *window);
    private:
        WinManagerPrivate& p;
        QWidget *window;
        void init();
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
    };
    class ResizeFrameEF :public QObject
    {
    public:

        ResizeFrameEF(WinManagerPrivate *parent):QObject(parent),p(*parent) {}
    private:
        WinManagerPrivate& p;
        bool resizing = false;
        bool eventFilter(QObject *sender, QEvent *event) override;
    };
};

void resizePaintFun(const QWidget *win,QPainter &p);
void snapPaintFun(const QWidget *win,QPainter &p);

WinManagerPrivate::WinManagerPrivate(WinManager *manager,QWidget *win):QObject(manager)
{
    this->manager = manager;
    window = win;
    window->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowMinMaxButtonsHint);
    window->installEventFilter(this);
    maximizeButton = minimizeButton = quitButton = nullptr;
    resizeRect = nullptr;
    frame = nullptr;
    currentSnapSide = pside = Side::none;

    frame = new QWidget(window);
    frame->installEventFilter(new WinManagerPrivate::ResizeFrameEF(this));
    frame->setMouseTracking(true);

    f_start = true;
    f_moving = false;
    movingCursor = window->cursor();
    oldCursor = window->cursor();
    resizePaintFunc = resizePaintFun;
    snapPaintFunc = snapPaintFun;
    maximizedButtonProperty = "isMaximized";
    defaultGeometry = QRect(dsk.width()/2-window->width()/2,dsk.height()/2-window->height()/2,window->width(),window->height());
    borderWidth = 3;
    flags = SaveGeometry|DrawResizeRect|HalfSnap;
    sideSnapSides = Side::left|Side::right|bottom|bottom_left|bottom_right|top_left|top_right|top;
    maximizeSides = none;
    movingArea = 0;
    connect(QGuiApplication::primaryScreen(),&QScreen::availableGeometryChanged,this,&WinManagerPrivate::adjustSnap);
}

WinManagerPrivate::~WinManagerPrivate()
{
    saveWindowGeometry();
}

bool WinManagerPrivate::eventFilter(QObject *sender, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::Resize:
        updateFrameMask();
        break;
    case QEvent::MouseMove:
        if(f_moving)
            moveWindow();
        break;
    case QEvent::MouseButtonPress:
        if(static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
            checkMousePress();
        break;
    case QEvent::MouseButtonRelease:
        if(static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
            checkMouseRelease();
        break;
    case QEvent::ChildAdded:
        updateFrameMask();
        if(frame)
            frame->raise();
        break;
    case QEvent::WindowStateChange:
    {
        if(static_cast<QWindowStateChangeEvent*>(event)->oldState() == Qt::WindowMaximized && window->windowState() != Qt::WindowMinimized) {
            window->setGeometry(oldWindowGeometry);
            updateFrameMask();
        }
        if(maximizeButton) {
            maximizeButton->setProperty(maximizedButtonProperty,window->isMaximized());
            maximizeButton->setStyleSheet(maximizeButton->styleSheet());
        }
        break;
    }
    case QEvent::Show:
    {
        if(f_start) {
            f_start = false;
            loadWindowGeometry();
        }
        adjustSnap();
        updateFrameMask();
        break;
    }
    default:break;
    }
    return QObject::eventFilter(sender,event);
}
bool WinManagerPrivate::ResizeFrameEF::eventFilter(QObject *sender, QEvent *event)
{
    Q_UNUSED(sender)
    QMouseEvent *ev = static_cast<QMouseEvent *>(event);
    if(ev->type() == QEvent::MouseMove) {
        if(resizing) {
            p.resizeWindowToCursor(p.window);
            if(!p.resizeRect  && getDesktopSide(cr) == top){
                p.resizeRect = new ResizeRect(&p,QRect(p.window->x(),0,p.window->width(),dsk.height()));
                p.window->raise();
            }
            else if (p.resizeRect && getDesktopSide(cr) != top)
                p.deleteResizeRect();
        }
        else if (ev->buttons() == Qt::NoButton)
            p.updateCursor(p.frame);
    }
    else if(ev->buttons() == Qt::LeftButton && (ev->type() == QEvent::MouseButtonDblClick || ev->type() == QEvent::MouseButtonPress)) {
        resizing = true;
        p.captureSide = p.getWindowSide(cr);
        p.maxX = p.window->x()+p.window->width()-p.window->minimumWidth();
        p.maxY = p.window->y()+p.window->height()-p.window->minimumHeight();
        p.offset = getOffset(p.window,p.captureSide,cr);
        if(p.manager->testFlag(DrawResizeRect)){
            p.resizeRect = new ResizeRect(&p,p.window);
            emit p.manager->resizeFrameClicked();
        }
    }
    else if(ev->type() == QEvent::MouseButtonRelease) {
        if(ev->buttons() == Qt::NoButton)
            p.updateCursor(p.frame);
        if(ev->button() != Qt::LeftButton)
            return true;
        if(p.resizeRect)
        {
            p.window->setGeometry(p.resizeRect->geometry());
            p.deleteResizeRect();
        }
        resizing = false;
    }
    return true;
}

void WinManagerPrivate::updateFrameMask()
{
    if(!frame) return;
    if(window->isMaximized()) {
        frame->resize(0,0);
        return;
    }
    frame->setGeometry(0,0,window->width(),window->height());
    QRegion reg(frame->geometry());
    frame->clearMask();
    switch (currentSnapSide)
    {
    case Side::left:
        frame->setGeometry(window->width()-borderWidth,0,borderWidth,window->height());
        break;
    case Side::right:
        frame->setGeometry(0,0,borderWidth,window->height());
        break;
    case Side::top:
        frame->setGeometry(0,window->height()-borderWidth,window->width(),borderWidth);
        break;
    case Side::bottom:
        frame->setGeometry(0,0,window->width(),borderWidth);
        break;
    case Side::top_left:
        reg -= QRect(0,0,window->width()-borderWidth,window->height()-borderWidth);
        break;
    case Side::top_right:
        reg -= QRect(borderWidth,0,window->width()-borderWidth,window->height()-borderWidth);
        break;
    case Side::bottom_left:
        reg -= QRect(0,borderWidth,window->width()-borderWidth,window->height()-borderWidth);
        break;
    case Side::bottom_right:
        reg -= QRect(borderWidth,borderWidth,window->width()-borderWidth,window->height()-borderWidth);
        break;
    default:
        reg -= QRect(borderWidth,borderWidth,window->width()-borderWidth*2,window->height()-borderWidth*2);
        break;
    }
    frame->setMask(reg);
}

void WinManagerPrivate::checkMouseRelease()
{
    if(f_moving) {
        window->setCursor(oldCursor);
        updateCursor(frame);
        f_moving = false;
    }
    if(resizeRect) {
        // When constructing #rsrect via the constructor for side snap
        // it does not catch mouse events and does not remove itself in the mouseRelese method
        Side s = getDesktopSide(cr);
        if(maximizeSides&s)
            window->setWindowState(Qt::WindowMaximized);
        else {
            snapWindow(window,s);
            currentSnapSide = s;
        }
        updateFrameMask();
        deleteResizeRect();
        frame->setAttribute(Qt::WA_SetCursor, false);
    }
}

void WinManagerPrivate::checkMousePress()
{
    if(!movingArea || cr.y() < window->y() + movingArea) {
        f_moving = true;
        oldCursor = window->cursor();
        ppos = window->mapFromGlobal(cr);
    }
    if(!window->isMaximized() && !windowIsSnapped())
        oldWindowGeometry = window->geometry();
}

void WinManagerPrivate::moveWindow()
{
    window->setCursor(movingCursor);
    if(window->isMaximized()) {
        window->setWindowState(Qt::WindowNoState);
        window->setGeometry(cr.x()-oldWindowGeometry.width()/2,cr.y()-borderWidth*2,oldWindowGeometry.width(),oldWindowGeometry.height());
        adjustWinForDesktop();
        ppos = QPoint(window->mapFromGlobal(cr));
    }
    if(windowIsSnapped()) {
        window->setGeometry(oldWindowGeometry);
        window->move(cr.x()-window->width()/2,cr.y()-borderWidth);
        adjustWinForDesktop();
        ppos = QPoint(window->mapFromGlobal(cr));
        clearSideSnap();
        updateFrameMask();
    }
    else
    {
        Side side = getDesktopSide(cr);
        if (pside != side)
        {
            pside = side;
            if(!resizeRect)
                resizeRect = new ResizeRect(this,QRect());
            if(side &maximizeSides)
                resizeRect->setGeometry(dsk);
            else if(side&sideSnapSides) {
                resizeRect->setGeometry(window->geometry());
                snapWindow(resizeRect,side);
            }
            else {
                deleteResizeRect();
                pside = Side::none;
            }
        }
    }
    window->move(cr-ppos);
}

void WinManagerPrivate::saveWindowGeometry()
{
    QSettings setting(QCoreApplication::organizationName(), QCoreApplication::applicationName(),manager);
    setting.beginGroup(window->objectName());
    if(windowIsSnapped() || window->isMaximized())
        setting.setValue(S_GEOMETRY,oldWindowGeometry);
    else
        setting.setValue(S_GEOMETRY,window->geometry());
    setting.endGroup();
}

void WinManagerPrivate::loadWindowGeometry()
{
    if(!manager->testFlag(SaveGeometry))
        return;
    if(QCoreApplication::organizationName().isEmpty())
        QCoreApplication::setOrganizationName(QCoreApplication::applicationName());
    QSettings setting(QCoreApplication::organizationName(), QCoreApplication::applicationName(),manager);
    setting.beginGroup(window->objectName());      
    window->setGeometry(setting.value(S_GEOMETRY,defaultGeometry).toRect());
    adjustWinForDesktop();
    adjustSnap();
    setting.endGroup();}

void WinManagerPrivate::adjustWinForDesktop()
{
    if(window->x() < dsk.x())
        window->move(dsk.x(),window->y());
    else if(window->x()+window->width() > dsk.x()+dsk.width())
        window->move(dsk.x()+dsk.width()-window->width(),window->y());

    if(window->y() < dsk.y())
        window->move(window->x(),dsk.y());
    else if(window->y()+window->height() > dsk.y()+dsk.height())
        window->move(window->x(),dsk.y()+dsk.height()-window->height());
}

void WinManagerPrivate::deleteResizeRect()
{
    if(resizeRect) {
        delete resizeRect;
        resizeRect = nullptr;
    }
}

void WinManagerPrivate::snapWindow(QWidget*window,Side side)
{
    const QRect &ds = dsk;
    QSize s = window->size();
    if(manager->testFlag(WM::HalfSnap))
        s = QSize(ds.width()/2,ds.height()/2);

    switch (side) {
    case Side::top:
        window->setGeometry(ds.x(),ds.y(),ds.width(),s.height());
        break;
    case Side::bottom:
        window->setGeometry(ds.x(),ds.y()+ds.height()-s.height(),ds.width(),s.height());
        break;
    case Side::left:
        window->setGeometry(ds.x(),ds.y(),s.width(),ds.height());
        break;
    case Side::right:
        window->setGeometry(ds.x()+ds.width()-s.width(),ds.y(),s.width(),ds.height());
        break;
    case Side::top_left:
        window->setGeometry(ds.x(),ds.y(),s.width(),s.height());
        break;
    case Side::top_right:
        window->setGeometry(ds.x()+ds.width()-s.width(),ds.y(),s.width(),s.height());
        break;
    case Side::bottom_left:
        window->setGeometry(ds.x(),ds.y()+ds.height()-s.height(),s.width(),s.height());
        break;
    case Side::bottom_right:
        window->setGeometry(ds.x()+ds.width()-s.width(),ds.y()+ds.height()-s.height(),s.width(),s.height());
        break;
    default: break;
    }
}

void WinManagerPrivate::adjustSnap(const QRect& rect)
{
    if(window->isMaximized()) {
        window->setGeometry(rect);
        if(window->windowState() != Qt::WindowMinimized)
            window->setWindowState(Qt::WindowState::WindowMaximized);
    }
    else
        snapWindow(window,currentSnapSide);
    updateFrameMask();
}

Side WinManagerPrivate::getWindowSide(const QPoint& p) const
{
    Side s = Side::none;
    if(QRect(window->x(),window->y()+borderWidth,borderWidth,window->height()-borderWidth*2).contains(p))
        s =  Side::left;
    else if(QRect(window->x()+window->width()-borderWidth,window->y()+borderWidth,borderWidth,window->height()-borderWidth*2).contains(p))
        s =  Side::right;
    else if(QRect(window->x()+borderWidth,window->y(),window->width()-borderWidth*2,borderWidth).contains(p))
        s =  top;
    else if(QRect(window->x()+borderWidth,window->y()+window->height()-borderWidth,window->width()-borderWidth*2,borderWidth).contains(p))
        s =  bottom;
    else if(QRect(window->x(),window->y(),borderWidth,borderWidth).contains(p))
        s =  Side::top_left;
    else if(QRect(window->x()+window->width()-borderWidth,window->y(),borderWidth,borderWidth).contains(p))
        s =  Side::top_right;
    else if(QRect(window->x()+window->width()-borderWidth,window->y()+window->height()-borderWidth,borderWidth,borderWidth).contains(p))
        s =  Side::bottom_right;
    else if(QRect(window->x(),window->y()+window->height()-borderWidth,borderWidth,borderWidth).contains(p))
        s =  Side::bottom_left;

    if(currentSnapSide != none && s != none)
    {
       if(currentSnapSide == top)
           return  bottom;
       if (currentSnapSide == bottom)
           return top;
       if (currentSnapSide == Side::right)
           return Side::left;
       if (currentSnapSide == Side::left)
           return Side::right;
       if (currentSnapSide == top_left) {
           if (s == bottom_left)
               s = bottom;
           if (s == top_right)
               s = Side::right;
       }
       if (currentSnapSide == top_right) {
           if (s == top_left)
               return Side::left;
           if (s == bottom_right)
               return Side::bottom;
       }
       if (currentSnapSide == bottom_left) {
           if (s == top_left)
               return Side::top;
           if (s == bottom_right)
               return Side::right;
       }
       if (currentSnapSide == bottom_right) {
           if (s == bottom_left)
               return Side::left;
           if (s == top_right)
               return Side::top;
       }
    }
    return s;
}

void WinManagerPrivate::updateCursor(QWidget *widget)
{
    switch (getWindowSide(cr))
    {
    case Side::left:
    case Side::right:
        widget->setCursor(Qt::SizeHorCursor);
        break;
    case top:
    case bottom:
        widget->setCursor(Qt::SizeVerCursor);
        break;
    case Side::bottom_right:
    case Side::top_left:
        widget->setCursor(Qt::SizeFDiagCursor);
        break;
    case Side::bottom_left:
    case Side::top_right:
        widget->setCursor(Qt::SizeBDiagCursor);
        break;
    case Side::none:
        break;
    }
}

QPoint WinManagerPrivate::getOffset(QWidget *window, Side side, const QPoint &pos)
{
    QPoint offset(0,0);
    if(side == Side::left || side == Side::top_left || side == Side::bottom_left)
        offset.setX( pos.x()-window->x());
    if(side == Side::right || side == Side::top_right || side == Side::bottom_right)
        offset.setX( pos.x()-(window->x()+window->width()));
    if(side == top || side == Side::top_left || side == Side::top_right)
        offset.setY( pos.y()-window->y());
    if(side == bottom ||side == Side::bottom_left || side == Side::bottom_right)
        offset.setY( pos.y()-(window->y()+window->height()));
    return offset;
}

void WinManagerPrivate::resizeWindowToCursor(QWidget *window)
{
    QPoint p = cr-offset;
    switch (captureSide) {
    case Side::right:
        window->resize(p.x()-window->x(),window->height());
        break;
    case bottom:
        window->resize(window->width(),p.y()-window->y());
        break;
    case Side::bottom_right:
        window->resize(p.x()-window->x(),p.y()-window->y());
        break;
    case Side::left:
        adjustX(p);
        window->setGeometry(p.x(),window->y(),window->x()+window->width()-p.x(),window->height());
        break;
    case top:
        adjustY(p);
        window->setGeometry(window->x(),p.y(),window->width(),window->y()+window->height()-p.y());
        break;
    case Side::top_right:
        adjustY(p);
        window->setGeometry(window->x(),p.y(),p.x()-window->x(),window->y()+window->height()-p.y());
        break;
    case Side::bottom_left:
        adjustX(p);
        window->setGeometry(p.x(),window->y(),window->x()+window->width()-p.x(),p.y()-window->y());
        break;
    case Side::top_left:
        adjustX(p);
        adjustY(p);
        window->setGeometry(p.x(),p.y(),window->x()+window->width()-p.x(),window->y()+window->height()-p.y());
        break;
    case Side::none:
        break;
    }
}

Side WinManagerPrivate::getDesktopSide(const QPoint &point)
{
    if(point.x() <= dsk.x() && point.y() <= dsk.y())
        return Side::top_left;
    if(point.x() >= dsk.x()+dsk.width()-1 && point.y() <= dsk.y())
        return Side::top_right;
    if(point.x() <= dsk.x() && point.y() >= dsk.y()+dsk.height()-1)
        return Side::bottom_left;
    if(point.x() >= dsk.x()+dsk.width()-1 && point.y() >= dsk.y()+dsk.height()-1)
        return Side::bottom_right;
    if(point.x() == dsk.x() && point.y() == dsk.y())
        return Side::top_left;
    if(point.x() <= dsk.x())
        return Side::left;
    if(point.x() >= dsk.x()+dsk.width()-1)
        return Side::right;
    if(point.y() <= dsk.y())
        return top;
    if(point.y() >= dsk.y()+dsk.height()-1)
        return bottom;

    return Side::none;
}

void resizePaintFun(const QWidget *win,QPainter &p)
{
    QPen pn;
    pn.setWidth(5);
    pn.setColor(Qt::black);
    pn.setJoinStyle(Qt::MiterJoin);
    p.setPen(pn);
    p.drawRect(2,2,win->width()-5,win->height()-5);
}

void snapPaintFun(const QWidget *win,QPainter &p)
{
    p.setPen(QPen(Qt::NoPen));
    p.setBrush(QColor(107, 157, 250,80));
    p.drawRect(0,0,win->width(),win->height());
}

void WinManagerPrivate::ResizeRect::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    p.paintFunc(this,painter);
}

WinManagerPrivate::ResizeRect::ResizeRect(WinManagerPrivate* parent,QWidget *win):p(*parent)
{
    init();
    window = win;
    p.paintFunc = p.resizePaintFunc;

    setMinimumSize(window->minimumSize());
    setGeometry(window->geometry());
    setMouseTracking(true);
    p.updateCursor(this);
    grabMouse();
    setFocus();
}

void WinManagerPrivate::ResizeRect::init()
{
    setAttribute(Qt::WA_TranslucentBackground,true);
    setWindowFlags(Qt::FramelessWindowHint|Qt::Tool);
    show();
    emit p.manager->sideSnapRectCreated();
}

WinManagerPrivate::ResizeRect::ResizeRect(WinManagerPrivate* parent, const QRect &rect):p(*parent)
{
    init();
    p.paintFunc = p.snapPaintFunc;
    setGeometry(rect);
}

void WinManagerPrivate::ResizeRect::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() != Qt::LeftButton) return;
    QApplication::sendEvent(p.frame,event);
    window->setGeometry(geometry());
    p.deleteResizeRect();
}

void WinManagerPrivate::ResizeRect::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    p.resizeWindowToCursor(this);
}

WinManager::WinManager(QWidget *window):QObject (window),p(new WinManagerPrivate(this,window)) { }

WinManager::~WinManager() { }


void WinManagerPrivate::minimizeWindow()
{
    window->setWindowState(Qt::WindowMinimized|window->windowState());
}
void WinManagerPrivate::maximizeWindow()
{
    if(window->isMaximized())
        window->setWindowState(Qt::WindowState::WindowNoState);
    else
        window->setWindowState(Qt::WindowState::WindowMaximized);
    if(!windowIsSnapped())
        oldWindowGeometry = window->geometry();
    clearSideSnap();
}

void WinManagerPrivate::quitApp()
{
    QApplication::quit();
}

bool WinManager::setMinimizeButton(QAbstractButton *button)
{
    if(button && button != p->quitButton && button != p->maximizeButton) {
        p->minimizeButton = button;
        return connect(p->minimizeButton,&QAbstractButton::clicked,p,&WinManagerPrivate::minimizeWindow);
    }
    return false;
}

bool WinManager::setMaximizeButton(QAbstractButton *button)
{
    if(button && button != p->quitButton && button != p->minimizeButton) {
        p->maximizeButton = button;
        p->maximizeButton->setProperty(maximizeButtonProperty(),p->window->isMaximized());
        return connect(p->maximizeButton,&QAbstractButton::clicked,p,&WinManagerPrivate::maximizeWindow);
    }
    return false;
}

bool WinManager::setQuitButton(QAbstractButton *button)
{
    if(button && button != p->minimizeButton && button != p->maximizeButton) {
        p->quitButton = button;
        return connect(p->quitButton,&QAbstractButton::clicked,p,&WinManagerPrivate::quitApp);
    }
    return false;
}

bool WinManager::disconnectMinimizeButton()
{
    if(p->minimizeButton) {
        bool f = disconnect(p->minimizeButton,&QAbstractButton::clicked,p,&WinManagerPrivate::minimizeWindow);
        p->minimizeButton = nullptr;
        return f;
    }
    return false;
}

bool WinManager::disconnectMaximizeButton()
{
    if(p->maximizeButton) {
        bool f = disconnect(p->maximizeButton,&QAbstractButton::clicked,p,&WinManagerPrivate::maximizeWindow);
        p->maximizeButton = nullptr;
        return f;
    }
    return false;
}

bool WinManager::disconnectQuitButton()
{
    if(p->quitButton) {
        bool f = disconnect(p->quitButton,&QAbstractButton::clicked,p,&WinManagerPrivate::quitApp);
        p->quitButton = nullptr;
        return f;
    }
    return false;
}

void WinManager::showResizeFrame(const QColor &color)
{
    //p->frame = new QWidget(p->window);
    p->frame->setAutoFillBackground(true);
    p->frame->setPalette(QPalette(color));
}

void WinManager::setMaximizeButtonProperty(const char *str)
{
    p->maximizedButtonProperty = str;
    if(p->maximizeButton)
        p->maximizeButton->setProperty(p->maximizedButtonProperty,p->window->isMaximized());
}

int WinManager::borderWidth() const
{ return p->borderWidth; }

void WinManager::setBorderWidth(int value)
{ p->borderWidth = value; }

QRect WinManager::defaultGeometry() const
{ return p->defaultGeometry; }

void WinManager::setDefaultGeometry(const QRect &value)
{ p->defaultGeometry = value; }

int WinManager::movementArea() const
{ return p->movingArea; }

void WinManager::setMovemenArea(int value)
{ p->movingArea = value; }

const char *WinManager::maximizeButtonProperty() const
{ return p->maximizedButtonProperty; }

void WinManager::setMoveCursor(const QCursor &cursor)
{ p->movingCursor = cursor;}

void WinManager::overrideMaximizeSides(Sides sides)
{ p->maximizeSides = sides; p->sideSnapSides &= ~sides; }

void WinManager::overrideSideSnapSides(Sides sides)
{ p->sideSnapSides = sides; p->maximizeSides &= ~sides; }

Flags WinManager::getFlags()
{ return p->flags; }

void WinManager::setFlags(Flags flags)
{ p->flags |= flags; }

void WinManager::overrideFlags(Flags flags)
{ p->flags = flags; }

void WinManager::disableFlags(Flags flags)
{ p->flags &= ~flags; }

bool WinManager::testFlag(Flag flag)
{ return p->flags&flag; }

void WinManager::setSnapPaintFunction(PaintFunction func)
{ p->snapPaintFunc = func; }

void WinManager::setResizePaintFunction(PaintFunction func)
{ p->resizePaintFunc = func; }
