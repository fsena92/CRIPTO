#include<stdexcept>
#include<iostream>
#include"OGLWindow.h"

OGLWindow::OGLWindow(OGLWindowDescription description)
{
    //TODO
    //NO SE ESTA CAPTURANDO LOS ERRORES GENERADOS POR X11
    //FALTA AGREGAR ICONO A LA VENTANA
    /******************************************************************************/
    mDisplay = XOpenDisplay(NULL);
    if (!mDisplay) {
        throw std::runtime_error("cant open display");
    }
    /******************************************************************************/
    int glxMajor;
    int glxMinor;
    if (!glXQueryVersion(mDisplay, &glxMajor, &glxMinor)) {
        XCloseDisplay(mDisplay);
        throw std::runtime_error("glXQueryVersion failed");
    }

    if ((glxMajor == 1 && glxMinor < 3) || glxMajor < 1) {
        XCloseDisplay(mDisplay);
        throw std::runtime_error("glX 1.3 or higher version is required");
    }
    /******************************************************************************/
    int attr[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        None
    };

    //EXPRESAR MEJOR LA CAUSA DE FALLO
    //INFORMAR CUANDO fbcount ES CERO O
    //CUANDO fbconfig ES NULL
    int fbcount = 0;
    GLXFBConfig* fbconfig = glXChooseFBConfig(mDisplay, DefaultScreen(mDisplay), attr, &fbcount);
    if (!fbconfig || !fbcount) {
        XCloseDisplay(mDisplay);
        throw std::runtime_error("glXChooseFBConfig failed");
    }

    int indx = -1;
    for (int i = 0; i < fbcount; i++) {
        int caveat = 0;
        if (glXGetFBConfigAttrib(mDisplay, fbconfig[i], GLX_CONFIG_CAVEAT, &caveat) == Success) {
            if (caveat == GLX_NONE) {
                indx = i;
                mFBConfig = fbconfig[i];
                break;
            }
        }
    }

    if (indx < 0) {
        std::cout << "possible slow frame config selected" << std::endl;
        mFBConfig = fbconfig[0];
    }
    XFree(fbconfig);
    /******************************************************************************/
    XVisualInfo* visual = glXGetVisualFromFBConfig(mDisplay, mFBConfig);
    if (!visual) {
        XCloseDisplay(mDisplay);
        throw std::runtime_error("glXGetVisualFromFBConfig failed");
    }
    /******************************************************************************/
    mColormap = XCreateColormap(mDisplay, DefaultRootWindow(mDisplay), visual->visual, AllocNone);

    //HAY QUE REVISAR PORQUE backing_store GENERA "ARTEFACTOS"
    //EN LA PANTALLA
    XSetWindowAttributes wndAttr;
    wndAttr.border_pixel = 0;
    wndAttr.backing_store = Always;
    wndAttr.colormap = mColormap;
    wndAttr.event_mask = ExposureMask | StructureNotifyMask | FocusChangeMask |
                         KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask | EnterWindowMask | LeaveWindowMask;

    mWindow = XCreateWindow(mDisplay, DefaultRootWindow(mDisplay), 0, 0,
                            description.width, description.height, 0, visual->depth,
                            InputOutput, visual->visual, CWBorderPixel | CWBackingStore | CWColormap |
                            CWEventMask, &wndAttr);

    XFree(visual);
    if (!mWindow) {
        XFreeColormap(mDisplay, mColormap);
        XCloseDisplay(mDisplay);
        throw std::runtime_error("XCreateWindow failed");
    }
    /******************************************************************************/
    mGlxWindow = glXCreateWindow(mDisplay, mFBConfig, mWindow, NULL);
    if (!mGlxWindow) {
        XDestroyWindow(mDisplay, mWindow);
        XFreeColormap(mDisplay, mColormap);
        XCloseDisplay(mDisplay);
        throw std::runtime_error("glXCreateWindow failed");
    }
    /******************************************************************************/
    //TODO
    //FALTA VERIFICAR ATOMS
    //SE PODRIA ARMAR UNA FUNCION QUE SIRVA TAMBIEN PARA GETSTATE
    mWmProtocols = XInternAtom(mDisplay, "WM_PROTOCOLS", False);
    mWmDeleteWindow = XInternAtom(mDisplay, "WM_DELETE_WINDOW", False);
    mNetWMState = XInternAtom(mDisplay, "_NET_WM_STATE", False);
    mNetWMStateFullscreen = XInternAtom(mDisplay, "_NET_WM_STATE_FULLSCREEN", False);
    mNetWMStateMaximizedHorz = XInternAtom(mDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    mNetWMStateMaximizedVert = XInternAtom(mDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom atoms[2];
    atoms[0] = mWmDeleteWindow;

    mUseSync = false;
    if (epoxy_has_glx_extension(mDisplay, DefaultScreen(mDisplay), "GLX_INTEL_swap_event")) {

        int syncEventBase = 0;
        int syncErrorBase = 0;
        if (XSyncQueryExtension(mDisplay, &syncEventBase, &syncErrorBase)) {
            int syncMajor;
            int syncMinor;
            if (XSyncInitialize(mDisplay, &syncMajor, &syncMinor)) {
                //TODO
                //SI EL EVENTO SWAP COMPLETE ESTA EN UN CASE DEL SWITCH
                //QUE VALOR DEBERIA TOMAR PARA QUE CUANDO mUseSync ES FALSE
                //NUNCA SEA ACEPTADO?
                mGlxEventBase = INT_MIN;
                if (glXQueryExtension(mDisplay, NULL, &mGlxEventBase)) {

                    mNetWmSyncRequest = XInternAtom(mDisplay, "_NET_WM_SYNC_REQUEST", False);
                    mNetWmSyncRequestCounter = XInternAtom(mDisplay, "_NET_WM_SYNC_REQUEST_COUNTER", False);

                    XSyncValue value;
                    XSyncIntToValue(&value, 0);
                    mSyncCounter = XSyncCreateCounter(mDisplay, value);
                    if (mSyncCounter) {
                        atoms[1] = mNetWmSyncRequest;
                        mUseSync = true;
                        mHasReceivedSyncRequest = false;
                    }
                }
            }
        }
    }
    XSetWMProtocols(mDisplay, mWindow, atoms, mUseSync ? 2 : 1);

    if (mUseSync) {
        XChangeProperty(mDisplay, mWindow, mNetWmSyncRequestCounter, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&mSyncCounter, 1);
    }
    /******************************************************************************/
    XStoreName(mDisplay, mWindow, description.title.c_str());
    /******************************************************************************/
    //TODO
    //PORHAY CONVIENE HACER UNA FUNCION
    if (description.minWidth > 0 || description.minHeight > 0 ||
            description.maxWidth < UINT_MAX || description.maxHeight < UINT_MAX) {

        XSizeHints* hints = XAllocSizeHints();
        if (!hints) {
            glXDestroyWindow(mDisplay, mGlxWindow);
            XDestroyWindow(mDisplay, mWindow);
            XFreeColormap(mDisplay, mColormap);
            XCloseDisplay(mDisplay);
            throw std::runtime_error("XAllocSizeHints failed");
        }

        hints->flags = 0;
        if (description.minWidth > 0 || description.minHeight > 0) {
            hints->min_width = description.minWidth;
            hints->min_height = description.minHeight;
            hints->flags |= PMinSize;
        }
        if (description.maxWidth < UINT_MAX || description.maxHeight < UINT_MAX) {
            hints->max_width = description.maxWidth;
            hints->max_height = description.maxHeight;
            hints->flags |= PMaxSize;
        }

        XSetWMNormalHints(mDisplay, mWindow, hints);
        XFree(hints);
    }
    /******************************************************************************/
    mWindowWidth = -1;
    mWindowHeight = -1;
    mX = description.x;
    mY = description.y;
}

OGLWindow::~OGLWindow()
{
    if (mWindow) {
        if (mUseSync) {
            XSyncDestroyCounter(mDisplay, mSyncCounter);
        }
        glXDestroyWindow(mDisplay, mGlxWindow);
        XDestroyWindow(mDisplay, mWindow);
        XFreeColormap(mDisplay, mColormap);
        XCloseDisplay(mDisplay);
    }
}

void OGLWindow::show()
{
    XWindowAttributes attr;
    if (XGetWindowAttributes(mDisplay, mWindow, &attr)) {
        //FIXME
        //ES FUNCIONAL PERO PODRIA SER MEJOR
        //VERIFICAR SI ES NECESARIO LLAMAR A glXGetCurrentContext
        //CUANDO NO ESTA CREADO EL CONTEXTO
        if (mUseSync && glXGetCurrentContext()) {
            glXSelectEvent(mDisplay, mGlxWindow, GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
        }
        if (attr.map_state == IsUnmapped) {
            XMapWindow(mDisplay, mWindow);
            XMoveWindow(mDisplay, mWindow, mX, mY);
            XSync(mDisplay, True);
        }
    }
}

void OGLWindow::fullscreen()
{
    int state = getState();
    if (state != OGL_STATE_NONE && !(state & OGL_STATE_FULLSCREEN)) {
        //TODO
        //VERIFICAR Y BORRAR HINST
        //DESACTIVAR EL COMPOSITOR
        //CONTEMPLAR EL CASO DE VENTANA NO MAPEADA
        XEvent event;
        event.xany.type = ClientMessage;
        event.xclient.message_type = mNetWMState;
        event.xclient.format = 32;
        event.xclient.window = mWindow;
        event.xclient.data.l[0] = 1;
        event.xclient.data.l[1] = mNetWMStateFullscreen;
        event.xclient.data.l[3] = 0;

        XSendEvent(mDisplay, RootWindow(mDisplay, DefaultScreen(mDisplay)), 0,
                   SubstructureNotifyMask | SubstructureRedirectMask, &event);
        XSync(mDisplay, True);
    }
}

void OGLWindow::restore()
{
    int state = getState();
    if (state != OGL_STATE_NONE && (state & OGL_STATE_FULLSCREEN)) {
        //TODO
        //RESTABLECER HINTS
        //ELIMINAR SI FUESE NECESARIO MAX_VERT Y MAX _HORTZ
        //HACE FALTA ACTIVAR LA COMPOSICION?
        //CONTEMPLAR CASO DE VENTANA NO MAPEADA
        XEvent event;
        event.xany.type = ClientMessage;
        event.xclient.message_type = mNetWMState;
        event.xclient.format = 32;
        event.xclient.window = mWindow;
        event.xclient.data.l[0] = 0;
        event.xclient.data.l[1] = mNetWMStateFullscreen;
        event.xclient.data.l[3] = 0;

        XSendEvent(mDisplay, RootWindow(mDisplay, DefaultScreen(mDisplay)), 0,
                   SubstructureNotifyMask | SubstructureRedirectMask, &event);
        XSync(mDisplay, True);
    }
}

int OGLWindow::getState()
{
    //TODO
    //MEJORAR
    Atom type = None;
    int format = 0;
    unsigned long numProperties = 0;
    unsigned long after = 0;
    unsigned char* properties = NULL;
    int state = OGL_STATE_NONE;

    if (XGetWindowProperty(mDisplay, mWindow, mNetWMState, 0, LONG_MAX, False,
                           XA_ATOM, &type, &format, &numProperties, &after, &properties) != Success) {
        std::cout << "XGetWindowProperty failed" << std::endl;
        return state;
    }

    unsigned long* data = (unsigned long*)properties;
    for (unsigned long i = 0; i < numProperties; i++) {

        if (data[i] == mNetWMStateFullscreen) {
            state |= OGL_STATE_FULLSCREEN;
        } else if (data[i] == mNetWMStateMaximizedHorz) {
            state |= OGL_STATE_MAXIMIZED_HORZ;
        } else if (data[i] == mNetWMStateMaximizedVert) {
            state |= OGL_STATE_MAXIMIZED_VERT;
        }
    }

    XFree(properties);
    if (!(state & OGL_STATE_FULLSCREEN)) {
        state |= OGL_STATE_NORMAL;
    }

    return state;
}

void OGLWindow::getEvent(OGLWindowEvent& event)
{
    XEvent xEvent;
    XNextEvent(mDisplay, &xEvent);
    event.type = OGL_WINDOW_NONE;

    switch (xEvent.type) {
    case MotionNotify:
        event.type = OGL_WINDOW_MOUSE_MOVE;
        event.data.mouse.x = xEvent.xmotion.x;
        event.data.mouse.y = xEvent.xmotion.y;
        break;

    case ButtonPress:
    case ButtonRelease:
        if (xEvent.xbutton.button == 1) {
            event.type = (xEvent.type == ButtonPress) ? OGL_WINDOW_MOUSE_CLICK_DOWN : OGL_WINDOW_MOUSE_CLICK_UP;
            event.data.mouse.x = xEvent.xbutton.x;
            event.data.mouse.y = xEvent.xbutton.y;
        } else if (xEvent.xbutton.button == 4 || xEvent.xbutton.button == 5) {
            event.type = OGL_WINDOW_MOUSE_WHEEL;
            event.data.mouse.x = xEvent.xbutton.x;
            event.data.mouse.y = xEvent.xbutton.y;
            event.data.mouse.delta = xEvent.xbutton.button == 4 ? 1 : -1;
        }
        break;

    case Expose:
        if (!xEvent.xexpose.count) {
            event.type = OGL_WINDOW_EXPOSE;
        }
        break;

    case ConfigureNotify:
        if (xEvent.xconfigure.width != mWindowWidth || xEvent.xconfigure.height != mWindowHeight) {
            event.type = OGL_WINDOW_SIZE;
            mWindowWidth = xEvent.xconfigure.width;
            mWindowHeight = xEvent.xconfigure.height;
            event.data.size.width = xEvent.xconfigure.width;
            event.data.size.height = xEvent.xconfigure.height;
        }
        break;

    case FocusIn:
        event.type = OGL_WINDOW_FOCUS_SET;
        break;

    case FocusOut:
        event.type = OGL_WINDOW_FOCUS_RELEASE;
        break;

    case EnterNotify:
        event.type = OGL_WINDOW_MOUSE_ENTER;
        break;

    case LeaveNotify:
        event.type = OGL_WINDOW_MOUSE_LEAVE;
        break;

    case KeyPress:
        //TODO
        //FALTA CHAR
        event.type = OGL_WINDOW_KEY_UP;
        event.data.key.code = xEvent.xkey.keycode;
        break;

    case KeyRelease:
        //TODO
        //FALTA CHAR
        event.type = OGL_WINDOW_KEY_DOWN;
        event.data.key.code = xEvent.xkey.keycode;
        break;

    case ClientMessage:
        if (xEvent.xclient.message_type == mWmProtocols) {
            if ((Atom)xEvent.xclient.data.l[0] == mNetWmSyncRequest) {
                mSyncCounterLow = xEvent.xclient.data.l[2];
                mSyncCounterHigh = xEvent.xclient.data.l[3];
                mHasReceivedSyncRequest = true;
            } else if ((Atom)xEvent.xclient.data.l[0] == mWmDeleteWindow) {
                event.type = OGL_WINDOW_CLOSE;
            }
        }
        break;
    /******************************************************************************/
    //TODO
    //SE PODRIA MEJORAR? TRATANDO DE NO COLOCARLO A LO ULTIMO?
    //PUEDE SER QUE ASI COMO ESTA PROGRAMADO SYNC REQUEST NO SEA DEL TODO CORRECTO
    default:
        if (mUseSync && xEvent.type == mGlxEventBase + GLX_BufferSwapComplete) {
            GLXBufferSwapComplete* glxEvent = (GLXBufferSwapComplete*)&xEvent;

            if (glxEvent->type == GLX_EXCHANGE_COMPLETE_INTEL ||
                    glxEvent->type == GLX_COPY_COMPLETE_INTEL ||
                    glxEvent->type == GLX_FLIP_COMPLETE_INTEL) {

                if (mHasReceivedSyncRequest) {
                    XSyncValue value;
                    XSyncIntsToValue(&value, mSyncCounterLow, mSyncCounterHigh);
                    XSyncSetCounter(mDisplay, mSyncCounter, value);
                    mHasReceivedSyncRequest = false;
                }
            }
        }
        /******************************************************************************/
    }
}

bool OGLWindow::pendingEvent()
{
    return XPending(mDisplay) > 0;
}

void OGLWindow::getNativeWindow(OGLWindowHandle& window)
{
    window = mWindow;
}

void OGLWindow::getNativeDisplay(OGLDisplayHandle& display)
{
    display = mDisplay;
}

void OGLWindow::getNativeFBConfig(OGLFBConfig& fbconfig)
{
    fbconfig = mFBConfig;
}

void OGLWindow::getNativeGLXWindow(OGLXWindowHandle& window)
{
    window = mGlxWindow;
}

void OGLWindow::setTitle(std::string title)
{
    XStoreName(mDisplay, mWindow, title.c_str());
}
