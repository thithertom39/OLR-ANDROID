#pragma once

// Debug-only frame-stall tracing hooks (Android release builds).
// The implementation lives in device.cpp inside `namespace ft`.

#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)

namespace ft
{
// Slots for mainloop_mark(); must stay in sync with PH_MAIN_LOOP_* in device.cpp.
enum MainLoopSlot
{
    ML_PRE = 0,
    ML_EVENTS,
    ML_ACTIVATE,
    ML_POST,
};

void mainloop_mark(int slot, const char* tag);
void reset_mark(const char* tag);
void set_background(bool background);
void shutdown(); // SDL/frame thread, before engine resources are destroyed.
} // namespace ft

#define FT_ML(slot, tag) ::ft::mainloop_mark((slot), (tag))
#define FT_RESET(tag) ::ft::reset_mark(tag)
#define FT_BACKGROUND(background) ::ft::set_background(background)
#define FT_SHUTDOWN() ::ft::shutdown()

#else

#define FT_ML(slot, tag) ((void)0)
#define FT_RESET(tag) ((void)0)
#define FT_BACKGROUND(background) ((void)0)
#define FT_SHUTDOWN() ((void)0)

#endif
