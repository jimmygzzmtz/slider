/* pc_vi.c - video interface → SDL window swap + frame pacing */
#include "pc_platform.h"
#include "pc_profiler.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
/* Yield to the browser via Asyncify/JSPI until the next requestAnimationFrame
 * fires. Replaces emscripten_sleep(N) here because Chromium on Snapdragon
 * 8 Gen 3 phones throttles setTimeout aggressively when the page looks
 * idle between presses — a 4 ms sleep would actually take 100–200 ms,
 * producing a visible stutter on each press. RAF is bound to the
 * compositor and always fires each display frame, so a wasm tick can't
 * overrun the frame budget waiting for a delayed timer. */
EM_ASYNC_JS(void, pc_yield_raf, (), {
    await new Promise(function(resolve) {
        var done = false;
        var fire = function() { if (!done) { done = true; resolve(); } };
        requestAnimationFrame(fire);
        setTimeout(fire, 17);
    });
});

#endif

#define VI_TVMODE_NTSC_INT    0
#define VI_TVMODE_NTSC_DS     1
#define VI_TVMODE_PAL_INT     4
#define VI_TVMODE_MPAL_INT    8
#define VI_TVMODE_EURGB60_INT 20

static u32 retrace_count = 0;
u32 pc_frame_counter = 0;
static Uint64 frame_start_time = 0;
static Uint64 perf_freq = 0;

// 16667us = 60.0 Hz (NTSC).
u32 g_frame_limiter = 60; // 60Hz
static void (*vi_pre_callback)(u32) = NULL;
static void (*vi_post_callback)(u32) = NULL;

void VIInit(void) {
    if (g_frame_limiter > 0) {
        printf("[VI] frame limit=%luus (%lu Hz)\n",
               (u32)((1.0 / (double)g_frame_limiter) * 1000000), (unsigned long)g_frame_limiter);
    } else {
        printf("[VI] frame limit=disabled\n");
    }
}

void VIConfigure(void* rm) { (void)rm; }

void VISetNextFrameBuffer(void* fb) { (void)fb; }

void VIFlush(void) {}

void VIWaitForRetrace(void) {
    if (!perf_freq) perf_freq = SDL_GetPerformanceFrequency();

#ifdef TARGET_ANDROID
    if (g_pc_verbose) {
        static int vi_calls_this_sec = 0;
        static Uint64 vi_sec_start = 0;
        if (vi_sec_start == 0) vi_sec_start = SDL_GetPerformanceCounter();
        vi_calls_this_sec++;
        Uint64 now = SDL_GetPerformanceCounter();
        if ((now - vi_sec_start) * 1000 / SDL_GetPerformanceFrequency() >= 2000) {
            printf("[VI] %d swaps in 2s (%.1f swaps/sec) frame_counter=%u\n",
                   vi_calls_this_sec, vi_calls_this_sec / 2.0, pc_frame_counter);
            vi_calls_this_sec = 0;
            vi_sec_start = now;
        }
    }
#endif

    /* --- frame time diagnostic (only collected when verbose; printed below) --- */
    Uint64 vi_enter = 0, t_before_swap = 0, t_after_swap = 0;
    Uint64 t_before_pace = 0;
    double frame_ms = 0.0;
    if (g_pc_verbose) {
        vi_enter = SDL_GetPerformanceCounter();
        if (frame_start_time) {
            frame_ms = (double)(vi_enter - frame_start_time) * 1000.0 / (double)perf_freq;
        }
    }

    Uint64 t_before_poll = pc_profiler_begin_timer();
    if (!pc_platform_poll_events()) {
        g_pc_running = 0;
        return;
    }
    pc_profiler_add_time(PC_PROF_TIMER_POLL_EVENTS, t_before_poll);

    /* Drain the frame's last deferred batch here so its cost bills to
     * gx_flush instead of inflating the swap timer. */
    {
        extern void pc_gx_draw_pending(void);
        Uint64 t_drain = pc_profiler_begin_timer();
        pc_gx_draw_pending();
        pc_profiler_add_time(PC_PROF_TIMER_GX_FLUSH, t_drain);
    }

    if (g_pc_verbose) t_before_swap = SDL_GetPerformanceCounter();
    Uint64 t_before_swap_prof = pc_profiler_begin_timer();
    pc_platform_swap_buffers();
    pc_profiler_add_time(PC_PROF_TIMER_SWAP, t_before_swap_prof);
    if (g_pc_verbose) {
        t_after_swap = SDL_GetPerformanceCounter();
        t_before_pace = t_after_swap;
    }

    Uint64 t_before_pace_prof = pc_profiler_begin_timer();
    {
        extern int g_pc_nes_active;
        int pace_frame = g_pc_nes_active || g_frame_limiter > 0;
        int pace_us = 0;

        if (g_pc_nes_active) {
            pace_us = 16667;
        } else if (g_frame_limiter > 0) {
            pace_us = (int)((1.0 / (double)g_frame_limiter) * 1000000);
        }

        if (pace_frame) {
#ifdef __EMSCRIPTEN__
            /* Emscripten: yield to the browser via Asyncify so rAF can paint.
             * Audio is also pumped here since there's no producer thread. */
            extern void pc_audio_pump_if_needed(void);
            pc_audio_pump_if_needed();
            /* Yield to RAF as the browser-paced wakeup. Replaces emscripten_sleep
             * because Chromium on Snapdragon 8 Gen 3 was throttling setTimeout to
             * 100–200 ms between user inputs, producing a stutter on every press.
             * RAF fires reliably each compositor frame regardless of throttling.
             *
             * At 60 Hz that's a single RAF per frame; at 120 Hz we yield twice
             * (~8.3 ms each) to preserve the game's 60 fps logical clock. The
             * elapsed-time check makes this self-correcting on any refresh rate.
             *
             * This also avoids the iOS Safari scheduling issue where a timer wakeup
             * can drift badly while the page is briefly backgrounded or throttled. */
            if (!g_pc_no_framelimit) {
                if (frame_start_time) {
                    for (int i = 0; i < 4; i++) {
                        pc_yield_raf();
                        Uint64 now = SDL_GetPerformanceCounter();
                        Uint64 elapsed_us = (now - frame_start_time) * 1000000 / perf_freq;
                        if (elapsed_us >= (Uint64)pace_us) {
                            break;
                        }
                    }
                } else {
                    pc_yield_raf();
                }
            } else {
                pc_yield_raf();
            }
#else
            if (!g_pc_no_framelimit) {
                /* Timer-based pacing: sleep until the configured frame budget.
                 * Audio production runs on a dedicated thread and is no longer
                 * tied to game frame timing. */
                if (frame_start_time) {
                    Uint64 now = SDL_GetPerformanceCounter();
                    Uint64 elapsed_us = (now - frame_start_time) * 1000000 / perf_freq;
                    while (elapsed_us < (Uint64)pace_us) {
                        Uint64 remain_us = (Uint64)pace_us - elapsed_us;
                        if (remain_us > 2000) {
                            SDL_Delay(1);
                        }
                        now = SDL_GetPerformanceCounter();
                        elapsed_us = (now - frame_start_time) * 1000000 / perf_freq;
                    }
                }
            }
#endif
        }
    }
    pc_profiler_add_time(PC_PROF_TIMER_PACE, t_before_pace_prof);
    Uint64 t_after_pace = SDL_GetPerformanceCounter();
    double profile_frame_ms = frame_ms;
    if (frame_start_time) {
        profile_frame_ms = (double)(t_after_pace - frame_start_time) * 1000.0 / (double)perf_freq;
    }
    /* report slow frames (>20ms = missed 60fps by >4ms) */
    if (g_pc_verbose && frame_ms > 20.0) {
        Uint64 t_after_pace = SDL_GetPerformanceCounter();
        double swap_ms = (double)(t_after_swap - t_before_swap) * 1000.0 / (double)perf_freq;
        double pace_ms = (double)(t_after_pace - t_before_pace) * 1000.0 / (double)perf_freq;
        double work_ms = (double)(vi_enter - frame_start_time) * 1000.0 / (double)perf_freq;
        int audio_fill = pc_audio_get_buffer_fill();
        printf("[STUTTER] frame %lu: total=%.1fms work=%.1fms swap=%.1fms pace=%.1fms audio_fill=%d\n",
               (unsigned long)pc_frame_counter, frame_ms, work_ms - swap_ms - pace_ms, swap_ms, pace_ms, audio_fill);
    }

    pc_profiler_end_frame(profile_frame_ms, pc_audio_get_buffer_fill());

#ifndef __EMSCRIPTEN__
    {
        static Uint64 fps_start = 0;
        static int fps_count = 0;
        if (fps_start == 0) fps_start = SDL_GetPerformanceCounter();
        fps_count++;
        if (fps_count >= 60) {
            Uint64 now = SDL_GetPerformanceCounter();
            double secs = (double)(now - fps_start) / (double)perf_freq;
            double fps = (double)fps_count / secs;
            char title[64];
            snprintf(title, sizeof(title), PC_WINDOW_TITLE " - %.1f FPS", fps);
            SDL_SetWindowTitle(g_pc_window, title);
            fps_start = now;
            fps_count = 0;
        }
    }
#endif

    frame_start_time = SDL_GetPerformanceCounter();

    retrace_count++;
    pc_frame_counter++;
}

u32 VIGetRetraceCount(void) { return retrace_count; }

void VISetBlack(BOOL black) { (void)black; }

u32 VIGetTvFormat(void) { return 0; /* VI_NTSC */ }
u32 VIGetDTVStatus(void) { return 0; }

void* VISetPreRetraceCallback(void* cb) {
    void* old = (void*)vi_pre_callback;
    vi_pre_callback = (void (*)(u32))cb;
    return old;
}

void* VISetPostRetraceCallback(void* cb) {
    void* old = (void*)vi_post_callback;
    vi_post_callback = (void (*)(u32))cb;
    return old;
}

u32 VIGetCurrentLine(void) { return 0; }

void VISetNextXFB(void* xfb) { (void)xfb; }
