#pragma once

#if defined(XR_PLATFORM_ANDROID)
namespace android_app_lifecycle
{
// SDL's Android event pump waits for DIDENTERBACKGROUND to leave its queue
// before releasing the EGL context. Keep input/window events in their own queues.
struct State
{
    bool background = false;
    unsigned event_logs = 0;
    unsigned reset_logs = 0;
    bool app_queue_error_logged = false;
    bool reset_queue_error_logged = false;

    bool Drain()
    {
        SDL_Event events[32];
        // Bound one iteration even if a producer keeps appending events.
        for (unsigned batch = 0; batch < 8; ++batch)
        {
            const int count = SDL_PeepEvents(events, 32, SDL_GETEVENT,
                SDL_APP_WILLENTERBACKGROUND, SDL_APP_DIDENTERFOREGROUND);
            if (count < 0)
            {
                if (!app_queue_error_logged)
                {
                    app_queue_error_logged = true;
                    Msg("! [android_lifecycle] app event queue failed: %s", SDL_GetError());
                }
                return false;
            }
            for (int i = 0; i < count; ++i)
            {
                const Uint32 type = events[i].type;
                if (type == SDL_APP_WILLENTERBACKGROUND || type == SDL_APP_DIDENTERBACKGROUND)
                    background = true;
                else if (type == SDL_APP_DIDENTERFOREGROUND)
                    background = false;
                // WILLENTERFOREGROUND alone does not make rendering ready.
                if (event_logs < 16)
                {
                    ++event_logs;
                    // This reads SDL's thread-local context pointer, not GL state.
                    Msg("* [android_lifecycle] event=0x%x background=%u context=%p",
                        unsigned(type), unsigned(background), SDL_GL_GetCurrentContext());
                }
            }
            if (count < 32)
                return true;
        }
        // A newer background event may still follow the last processed batch.
        // Defer this frame until the next iteration drains the remaining queue.
        return false;
    }

    void DrainRenderResetDiagnostics()
    {
        // SDL can replace a genuinely lost EGL context and report this event.
        // A render-target reset cannot rebuild all engine textures/static
        // geometry/programs. Record the event without pretending that it can.
        SDL_Event events[8];
        const int count = SDL_PeepEvents(events, 8, SDL_GETEVENT,
            SDL_RENDER_TARGETS_RESET, SDL_RENDER_DEVICE_RESET);
        if (count < 0)
        {
            if (!reset_queue_error_logged)
            {
                reset_queue_error_logged = true;
                Msg("! [android_lifecycle] render reset event queue failed: %s", SDL_GetError());
            }
            return;
        }
        for (int i = 0; i < count; ++i)
        {
            if (reset_logs >= 4)
                continue;
            ++reset_logs;
            Msg("! [android_lifecycle] render_reset=0x%x context=%p full_context_recovery_unimplemented=%u",
                unsigned(events[i].type), SDL_GL_GetCurrentContext(),
                unsigned(events[i].type == SDL_RENDER_DEVICE_RESET));
        }
    }
};
} // namespace android_app_lifecycle
#endif
