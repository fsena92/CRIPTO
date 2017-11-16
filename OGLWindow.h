#include<X11/Xlib.h>
#include<X11/extensions/sync.h>
#include<X11/Xatom.h>
#include<epoxy/gl.h>
#include<epoxy/glx.h>
#include<climits>
#include<string>

#ifndef _OGL_WINDOW_H_
#define _OGL_WINDOW_H_

#define OGL_WINDOW_NONE 0X0000

#define OGL_WINDOW_EXPOSE 0X0001
#define OGL_WINDOW_SIZE 0X0002

#define OGL_WINDOW_FOCUS_RELEASE 0X0004
#define OGL_WINDOW_FOCUS_SET 0X0008

#define OGL_WINDOW_MOUSE_MOVE 0X0010
#define OGL_WINDOW_MOUSE_CLICK_UP 0X0020
#define OGL_WINDOW_MOUSE_CLICK_DOWN 0X0040
#define OGL_WINDOW_MOUSE_WHEEL 0X0080
#define OGL_WINDOW_MOUSE_ENTER 0X0100
#define OGL_WINDOW_MOUSE_LEAVE 0X0200

#define OGL_WINDOW_KEY_UP 0X0400
#define OGL_WINDOW_KEY_DOWN 0X0800
#define OGL_WINDOW_KEY_CHAR 0X1000

#define OGL_WINDOW_CLOSE 0X2000

struct OGLWindowDescription {
    std::string title;
    std::string icon;
    unsigned int width;
    unsigned int minWidth;
    unsigned int maxWidth;
    unsigned int height;
    unsigned int minHeight;
    unsigned int maxHeight;
    int x;
    int y;
    OGLWindowDescription()
    {
        title = "";
        icon = "";
        width = 0;
        minWidth = 0;
        maxWidth = UINT_MAX;
        height = 0;
        minHeight = 0;
        maxHeight = UINT_MAX;
        x = 0;
        y = 0;
    }
};

struct OGLWindowEvent {
    short type;
    union {
        struct {
            unsigned int width;
            unsigned int height;
        } size;
        struct {
            int x;
            int y;
            char delta;
        } mouse;
        struct {
            int code;
            char ch;
        } key;
    } data;
};

#define OGL_WINDOW_SIZE_GET_WIDTH(event) (event.data.size.width)
#define OGL_WINDOW_SIZE_GET_HEIGHT(event) (event.data.size.height)

#define OGL_WINDOW_MOUSE_GET_X(event) (event.data.mouse.x)
#define OGL_WINDOW_MOUSE_GET_Y(event) (event.data.mouse.y)
#define OGL_WINDOW_MOUSE_GET_DELTA(event) (event.data.mouse.delta)

#define OGL_WINDOW_KEY_GET_CODE(event) (event.data.key.code)
#define OGL_WINDOW_KEY_GET_CHAR(event) (event.data.key.ch)

enum OGL_WINDOW_STATE {
    OGL_STATE_NONE = 0,
    OGL_STATE_FULLSCREEN = 1,
    OGL_STATE_NORMAL = 2,
    OGL_STATE_MAXIMIZED_HORZ = 4,
    OGL_STATE_MAXIMIZED_VERT = 8
};

#ifdef __linux__
/**/
typedef Window OGLWindowHandle;
/**/
typedef Display* OGLDisplayHandle;
/**/
typedef GLXFBConfig OGLFBConfig;
/**/
typedef GLXWindow OGLXWindowHandle;
#endif

class OGLWindow
{
private:
    Display* mDisplay;
    GLXFBConfig mFBConfig;
    Colormap mColormap;
    Window mWindow;
    GLXWindow mGlxWindow;
    Atom mWmDeleteWindow;
    Atom mWmProtocols;
    Atom mNetWMState;
    Atom mNetWMStateFullscreen;
    Atom mNetWMStateMaximizedHorz;
    Atom mNetWMStateMaximizedVert;
    Atom mNetWmSyncRequest;
    Atom mNetWmSyncRequestCounter;
    XSyncCounter mSyncCounter;
    long mSyncCounterHigh;
    long mSyncCounterLow;
    bool mUseSync;
    bool mHasReceivedSyncRequest;
    int mGlxEventBase;
    int mWindowWidth;
    int mWindowHeight;
    int mX;
    int mY;

public:
    /**/
    OGLWindow(OGLWindowDescription description);
    /**/
    ~OGLWindow();

    /**/
    void show();
    /**/
    void fullscreen();
    /**/
    void restore();
    /**/
    int getState();

    /**/
    void getEvent(OGLWindowEvent& event);
    /**/
    bool pendingEvent();

    /**/
    void getNativeWindow(OGLWindowHandle& window);
    /**/
    void getNativeDisplay(OGLDisplayHandle& display);

#ifdef __linux__
    /**/
    void getNativeFBConfig(OGLFBConfig& fbconfig);
    /**/
    void getNativeGLXWindow(OGLXWindowHandle& window);
#endif

    /**/
    void setTitle(std::string title);
};

#endif
