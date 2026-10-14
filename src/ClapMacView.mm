#import <Cocoa/Cocoa.h>

extern "C" void* artilodyCreateClapParentView (int w, int h)
{
    NSView* v = [[NSView alloc] initWithFrame: NSMakeRect (0, 0, w, h)];
    [v setAutoresizesSubviews: YES];
    return (void*) v;
}

extern "C" void artilodyReleaseClapParentView (void* view)
{
    if (view != nullptr)
        [(NSView*) view release];
}
