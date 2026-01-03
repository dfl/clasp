// Platform-specific GUI embedding for macOS
#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>

extern "C" {

void clasp_embed_webview(void* parentHandle, void* webviewHandle, int width, int height) {
    if (!parentHandle || !webviewHandle) return;

    NSView* parentView = (__bridge NSView*)parentHandle;
    NSView* webviewView = (__bridge NSView*)webviewHandle;

    // Set frame to fill parent
    [webviewView setFrame:NSMakeRect(0, 0, width, height)];

    // Auto-resize with parent
    [webviewView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    // Add as subview
    [parentView addSubview:webviewView];
}

void clasp_resize_webview(void* webviewHandle, int width, int height) {
    if (!webviewHandle) return;

    NSView* webviewView = (__bridge NSView*)webviewHandle;
    [webviewView setFrame:NSMakeRect(0, 0, width, height)];
}

void clasp_remove_webview(void* webviewHandle) {
    if (!webviewHandle) return;

    NSView* webviewView = (__bridge NSView*)webviewHandle;
    [webviewView removeFromSuperview];
}

} // extern "C"

#endif // __APPLE__
