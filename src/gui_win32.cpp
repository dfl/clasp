#include <windows.h>

extern "C" {

void clasp_embed_webview(void *parentHandle, void *webviewHandle, int width,
                         int height) {
  if (!parentHandle || !webviewHandle)
    return;

  HWND parent = (HWND)parentHandle;
  HWND webview = (HWND)webviewHandle;

  // Set parent and style
  SetParent(webview, parent);
  SetWindowLong(webview, GWL_STYLE, WS_CHILD | WS_VISIBLE);

  // Resize to fill parent
  MoveWindow(webview, 0, 0, width, height, TRUE);
}

void clasp_resize_webview(void *webviewHandle, int width, int height) {
  if (!webviewHandle)
    return;

  HWND webview = (HWND)webviewHandle;
  MoveWindow(webview, 0, 0, width, height, TRUE);
}

void clasp_remove_webview(void *webviewHandle) {
  if (!webviewHandle)
    return;

  HWND webview = (HWND)webviewHandle;
  SetParent(webview, NULL);
}
}
