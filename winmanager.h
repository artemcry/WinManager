#ifndef WINMANAGER_H
#define WINMANAGER_H

#include <QMouseEvent>
#include <QDesktopWidget>
#include <QSettings>
#include <QApplication>
#include <QAbstractButton>
#include <QPainter>
#include <QScreen>
#include <QDebug>

namespace WM
{
enum Side
{
    none = 0,
    left = 1,
    right = 2,
    top = 4,
    bottom = 8,
    bottom_left = 16,
    bottom_right = 32,
    top_left = 64,
    top_right = 128
};
enum Flag
{
    DrawResizeRect = 1,       // Draw a rectangle when the window is resized
    SaveGeometry = 2,         // Save geometry after closing the application
    HalfSnap = 4              // Is necessary to resize to half of the screen during snapping
};
Q_DECLARE_FLAGS(Flags,Flag)
Q_DECLARE_FLAGS(Sides,Side)
typedef void(*PaintFunction)(const QWidget *transparent_widget_under_picture,QPainter &painter);
}
Q_DECLARE_OPERATORS_FOR_FLAGS (WM::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS (WM::Sides)

class WinManagerPrivate;
class  WinManager : public QObject
{
    /*
     Further the following definitions will be used:
     #frame:
     A transparent frame around the edges of the window that can use to resize the window.

     #rect:
     This is a rectangular, transparent area that is created when the window is resized,
     or when the mouse is on one of the sides of desktop when moving the window (to snap to it),
     and provided that the DrawResizeRect flag is active.
     If it is created to resize, then it resizes according to the position of the cursor.
     You can draw on it, for this you need to assign your own drawing functions for the desired event
     (snapping to edge desktop or resize)
     The first parameter to the function is a pointer to this area,
     and the second is the QPainter that draws the image.
    */
    Q_OBJECT
public:
    explicit WinManager(QWidget *window);
    ~WinManager() override;
    WinManager(const WinManager& src) = delete;
    WinManager& operator=(const WinManager &oth) = delete;

    bool setMinimizeButton(QAbstractButton *button);
    bool setMaximizeButton(QAbstractButton *button);
    bool setQuitButton(QAbstractButton *button);
    // Connecting the button to (Minimize | Maximize | Exit the application) operations.
    // Returns the success of the operation.

    bool disconnectMinimizeButton();
    bool disconnectMaximizeButton();
    bool disconnectQuitButton();
    // Remove connecting the button to (Minimize | Maximize | Exit the application) operations.
    // Returns the success of the operation.

    void showResizeFrame(const QColor &color);
    // Makes the #frame visible, and sets it to the specified color.

    int borderWidth() const;
    void setBorderWidth(int width);
    // (Get | Set ) width #frame

    QRect defaultGeometry() const;
    void setDefaultGeometry(const QRect &rect);

    int movementArea() const;
    void setMovemenArea(int width);
    // (Get | Set ) Height of the area, relative to the top of the window, in which the window can be moved.
    // If the value is 0, can move the window from any point in the window.

    const char *maximizeButtonProperty() const;
    void setMaximizeButtonProperty(const char *str);
    // (Get | Set ) Pseudo-state for the maximize button.
    // Accepts true if the window is maximized otherwise - false.
    // Example:
    // pushBtton->setStyleSheet("QPushButton#yourMaximizeButton[your_property="true"] { background: red;}");

    void setMoveCursor(const QCursor& cursor);
    // Set the cursor to be displayed when dragging the window.

    void overrideMaximizeSides(WM::Sides sides);
    // Overrides the desktop sides upon reaching which the window will be maximized.

    void overrideSideSnapSides(WM::Sides sides);
    // Overrides the desktop sides to which the window can be bound.

    WM::Flags getFlags();

    void setFlags(WM::Flags flags);

    void overrideFlags(WM::Flags flags);

    void disableFlags(WM::Flags flags);

    bool testFlag(WM::Flag flag);

    void setSnapPaintFunction(WM::PaintFunction func);
    void setResizePaintFunction(WM::PaintFunction func);

signals:
    void resizeFrameClicked(); // Emitted on click on #frame
    void sideSnapRectCreated(); // Emitted when creating #rect to snap to desktop side
private:
    friend class WinManagerPrivate;
    WinManagerPrivate *p;
};

#endif
