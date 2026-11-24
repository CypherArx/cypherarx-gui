#include "platform/away.hpp"

#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <powrprof.h>
#include <wtsapi32.h>

namespace cypherarx::gui::platform {

struct AwayWatch::Impl {
    std::atomic<bool> resumed{false};
    HPOWERNOTIFY registration = nullptr;
};

namespace {

ULONG CALLBACK on_power(PVOID context, ULONG type, PVOID) {
    if (type == PBT_APMRESUMEAUTOMATIC || type == PBT_APMRESUMESUSPEND) {
        *static_cast<std::atomic<bool>*>(context) = true;
    }
    return ERROR_SUCCESS;
}

}

AwayWatch::AwayWatch() : impl_(std::make_unique<Impl>()) {
    DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS parameters{&on_power, &impl_->resumed};
    if (PowerRegisterSuspendResumeNotification(DEVICE_NOTIFY_CALLBACK, &parameters, &impl_->registration) !=
        ERROR_SUCCESS) {
        impl_->registration = nullptr;
    }
}

AwayWatch::~AwayWatch() {
    if (impl_->registration) {
        PowerUnregisterSuspendResumeNotification(impl_->registration);
    }
}

AwayState AwayWatch::poll() {
    AwayState state;
    BOOL running = FALSE;
    if (SystemParametersInfoW(SPI_GETSCREENSAVERRUNNING, 0, &running, 0)) {
        state.screensaver = running != FALSE;
    }
    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, &buffer,
                                    &bytes)) {
        if (bytes >= sizeof(WTSINFOEXW)) {
            const WTSINFOEXW* info = reinterpret_cast<const WTSINFOEXW*>(buffer);
            if (info->Level == 1) {
                state.screen_locked = info->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK;
                state.disconnected = info->Data.WTSInfoExLevel1.SessionState != WTSActive;
            }
        }
        WTSFreeMemory(buffer);
    }
    state.resumed = impl_->resumed.exchange(false);
    return state;
}

} // namespace cypherarx::gui::platform
