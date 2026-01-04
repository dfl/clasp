#if !defined(__APPLE__) && !defined(_WIN32)

#include <X11/Xlib.h>
#include <clap/clap.h>

extern "C" {

void clasp_embed_webview(void *parentHandle, void *webviewHandle, int width,
                         int height) {
  if (!parentHandle || !webviewHandle)
    return;

  // Assuming X11 for now
  Display *display = XOpenDisplay(NULL);
  if (!display)
    return;

  Window parent = (Window)parentHandle;
  Window webview = (Window)webviewHandle;

  XReparentWindow(display, webview, parent, 0, 0);
  XMapWindow(display, webview);
  XMoveResizeWindow(display, webview, 0, 0, width, height);
  XFlush(display);
  XCloseDisplay(display);
}

void clasp_resize_webview(void *webviewHandle, int width, int height) {
  if (!webviewHandle)
    return;

  Display *display = XOpenDisplay(NULL);
  if (!display)
    return;

  Window webview = (Window)webviewHandle;
  XMoveResizeWindow(display, webview, 0, 0, width, height);
  XFlush(display);
  XCloseDisplay(display);
}

void clasp_remove_webview(void *webviewHandle) {
  if (!webviewHandle)
    return;

  Display *display = XOpenDisplay(NULL);
  if (!display)
    return;

  Window webview = (Window)webviewHandle;
  XUnmapWindow(display, webview);
  XFlush(display);
  XCloseDisplay(display);
}
}

#endif
