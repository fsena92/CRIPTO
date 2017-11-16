#include<stdexcept>
#include"OGLUtils.h"

namespace utils
{

Display* privateOpenDisplay()
{
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        throw std::runtime_error("cant't open display");
    }
    return display;
}

void privateCloseDisplay(Display*& display)
{
    if (display) {
        XCloseDisplay(display);
        display = NULL;
    }
}

unsigned int getDesktopWidth()
{
    Display* display = privateOpenDisplay();
    int width = DefaultScreenOfDisplay(display)->width;
    privateCloseDisplay(display);
    return width;
}

unsigned int getDesktopHeight()
{
    Display* display = privateOpenDisplay();
    int height = DefaultScreenOfDisplay(display)->height;
    privateCloseDisplay(display);
    return height;
}

unsigned int getScreenWidth()
{
    Display* display = privateOpenDisplay();
    int width = DefaultScreenOfDisplay(display)->width;
    privateCloseDisplay(display);
    return width;
}

unsigned int getScreenHeight()
{
    Display* display = privateOpenDisplay();
    int height = DefaultScreenOfDisplay(display)->height;
    privateCloseDisplay(display);
    return height;
}

bool inToRect(int x, int y, int xLeftRect, int yTopRect, int xRightRect, int yBottomRect)
{
    return (x >= xLeftRect) && (x <= xRightRect) &&
           (y >= yTopRect) && (y <= yBottomRect);
}

bool inToCircle(int x, int y, int xCircle, int yCircle, int radiusCircle)
{
    /*(x-x0)*(x-x0) + (y-yo)*(y-yo) <= r*r*/
    int cx = x - xCircle;
    int cy = y - yCircle;
    return (cx*cx + cy*cy) <= radiusCircle*radiusCircle;
}

}
