#include<X11/Xlib.h>

#ifndef _OGL_UTILS_H_
#define _OGL_UTILS_H_

namespace utils
{
/**/
unsigned int getDesktopWidth();
/**/
unsigned int getDesktopHeight();
/**/
unsigned int getScreenWidth();
/**/
unsigned int getScreenHeight();

/**/
bool inToRect(int x, int y, int xLeftRect, int yTopRect, int xRightRect, int yBottomRect);
/**/
bool inToCircle(int x, int y, int xCircle, int yCircle, int radiusCircle);

}

#endif
