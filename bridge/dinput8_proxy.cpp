// Bully Co-op Hamdan | Position Probe v0.5 | Bully: Scholarship Edition (user-provided build only).
// READ-ONLY: does not patch the game, modify position, create NPCs, or enable multiplayer.
// This DLL is a DirectInput 8 proxy: it forwards actual input calls to the system DLL.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

static INIT_ONCE g_input_init = INIT_ONCE_STATIC_INIT;
static INIT_ONCE g_probe_init = INIT_ONCE_STATIC_INIT;
static HMODULE g_system_dinput8 = nullptr;
static const DWORD kExpectedBullySize = 8204288; // User's Bully.exe, SHA256 below in README.
static const uintptr_t kImageBase = 0x400000;
static const uintptr_t kPlayerGetPosRva = 0x1D2670;
// 0x5C7380 (called by native PlayerGetPosXYZ at 0x5D267F)
// returns the script-managed player actor directly from [0x00C1AEA8]
// for entity category 3. v0.4 incorrectly read the *separate* player manager.
static const uintptr_t kPlayerActorGlobalRva = 0x81AEA8; // VA 0x00C1AEA8 - image base.
static const char kPlayerGetPosSignature[] = "\x83\xec\x0c\x56\x51\x8b\xc4\x6a\x00\xc7\x00\x03\x00\x00\x00";

static void WriteProbeLog(const char* msg) {
    char path[MAX_PATH] = {};
    const DWORD used = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (used == 0 || used >= MAX_PATH) return;
    char* end = path;
    for (char* it = path; *it; ++it) {
        if (*it == '\\' || *it == '/') end = it + 1;
    }
    *end = '\0';
    const char name[] = "BullyCoop_bridge.log";
    if (static_cast<size_t>(end - path) + sizeof(name) > MAX_PATH) return;
    lstrcatA(path, name);
    HANDLE log = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return;
    DWORD ignored = 0;
    WriteFile(log, msg, static_cast<DWORD>(lstrlenA(msg)), &ignored, nullptr);
    CloseHandle(log);
}

static BOOL CALLBACK InitSystemInput(PINIT_ONCE, PVOID, PVOID*) {
    char path[MAX_PATH] = {};
    const UINT count = GetSystemDirectoryA(path, MAX_PATH);
    if (count == 0 || count > MAX_PATH - sizeof("\\dinput8.dll")) {
        WriteProbeLog("BullyCoop probe: unable to locate native system dinput8\r\n");
        return TRUE;
    }
    lstrcatA(path, "\\dinput8.dll");
    g_system_dinput8 = LoadLibraryA(path);
    WriteProbeLog(g_system_dinput8 ?
        "BullyCoop probe: DLL active; native system dinput8 loaded\r\n" :
        "BullyCoop probe: cannot load native system dinput8\r\n");
    return TRUE;
}

static FARPROC GetNativeProc(const char* name) {
    InitOnceExecuteOnce(&g_input_init, InitSystemInput, nullptr, nullptr);
    return g_system_dinput8 ? GetProcAddress(g_system_dinput8, name) : nullptr;
}

// Read memory through the Windows API: invalid addresses result in false, not a direct dereference.
template <typename T>
static bool ReadAt(uintptr_t address, T* value) {
    SIZE_T read = 0;
    if (address < 0x10000 || !value) return false;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                             value, sizeof(T), &read) != 0 && read == sizeof(T);
}

struct Position { float x, y, z; };

static bool VersionMatches(uintptr_t base) {
    if (base != kImageBase) {
        WriteProbeLog("BullyCoop position: unsupported relocated Bully.exe; disabled\r\n");
        return false;
    }
    char exe[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, exe, MAX_PATH)) return false;
    WIN32_FILE_ATTRIBUTE_DATA attributes = {};
    if (!GetFileAttributesExA(exe, GetFileExInfoStandard, &attributes)
        || attributes.nFileSizeHigh || attributes.nFileSizeLow != kExpectedBullySize) {
        WriteProbeLog("BullyCoop position: Bully.exe file size differs; disabled\r\n");
        return false;
    }
    char observed[sizeof(kPlayerGetPosSignature) - 1] = {};
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(base + kPlayerGetPosRva), observed,
        sizeof(observed), &read) || read != sizeof(observed)
        || memcmp(observed, kPlayerGetPosSignature, sizeof(observed)) != 0) {
        WriteProbeLog("BullyCoop position: PlayerGetPosXYZ code signature differs; disabled\r\n");
        return false;
    }
    return true;
}

// This is the character-pointer path used by the game native PlayerGetPosXYZ:
//   PlayerGetPosXYZ -> 0x5C7380(type=3) -> mov eax,[0xC1AEA8]
//   actor->0x1554 / actor->0x14 -> transform->0x30 (or actor+0x04).
// v0.4 read 0xD02850+0x6B84 as if it were the actor lookup; this is a
// DIFFERENT manager used later by PlayerGetPosXYZ and is often uninitialized.
// Read-only, with explicit failure reasons for on-device validation.
enum class PositionStatus {
    Good, GlobalEmpty, GlobalUnreadable, ActorInvalid, NestedUnreadable,
    TransformUnreadable, CoordinatesUnreadable, CoordinatesImplausible
};

static const char* StatusName(PositionStatus status) {
    switch (status) {
        case PositionStatus::Good: return "ready";
        case PositionStatus::GlobalEmpty: return "player actor global is empty (menu/loading?)";
        case PositionStatus::GlobalUnreadable: return "unable to read actor global";
        case PositionStatus::ActorInvalid: return "actor pointer looks invalid";
        case PositionStatus::NestedUnreadable: return "unable to read actor nested pointer";
        case PositionStatus::TransformUnreadable: return "unable to read transform pointer";
        case PositionStatus::CoordinatesUnreadable: return "unable to read coordinates";
        case PositionStatus::CoordinatesImplausible: return "coordinates are not finite/in expected range";
    }
    return "unknown";
}

static PositionStatus ReadJimmyPosition(uintptr_t base, Position* out) {
    if (!out) return PositionStatus::CoordinatesUnreadable;
    uint32_t actor = 0;
    if (!ReadAt(base + kPlayerActorGlobalRva, &actor)) return PositionStatus::GlobalUnreadable;
    if (!actor) return PositionStatus::GlobalEmpty;
    if (actor < 0x10000u || actor > 0x7FFFFFFFu) return PositionStatus::ActorInvalid;

    uint32_t nested = 0;
    if (!ReadAt(static_cast<uintptr_t>(actor) + 0x1554u, &nested)) return PositionStatus::NestedUnreadable;
    uint32_t transform = 0;
    uintptr_t address = 0;
    if (nested != 0) {
        if (!ReadAt(static_cast<uintptr_t>(nested) + 0x14u, &transform))
            return PositionStatus::TransformUnreadable;
        address = transform ? static_cast<uintptr_t>(transform) + 0x30u :
                              static_cast<uintptr_t>(nested) + 0x04u;
    } else {
        if (!ReadAt(static_cast<uintptr_t>(actor) + 0x14u, &transform))
            return PositionStatus::TransformUnreadable;
        address = transform ? static_cast<uintptr_t>(transform) + 0x30u :
                              static_cast<uintptr_t>(actor) + 0x04u;
    }
    if (!ReadAt(address, out)) return PositionStatus::CoordinatesUnreadable;
    if (!std::isfinite(out->x) || !std::isfinite(out->y) || !std::isfinite(out->z) ||
        std::fabs(out->x) >= 1000000.0f || std::fabs(out->y) >= 1000000.0f ||
        std::fabs(out->z) >= 1000000.0f)
        return PositionStatus::CoordinatesImplausible;
    return PositionStatus::Good;
}

static DWORD WINAPI PositionThread(LPVOID parameter) {
    const uintptr_t base = reinterpret_cast<uintptr_t>(parameter);
    if (!VersionMatches(base)) return 0;
    WriteProbeLog("BullyCoop position v0.5: corrected global-player actor lookup; read-only\r\n");
    bool previouslyAvailable = false;
    Position previous = {};
    DWORD lastLog = 0;
    DWORD lastWait = 0;
    PositionStatus previousStatus = PositionStatus::Good;
    while (true) {
        Sleep(250); // 4 reads / s; write the log only when moving or every 10 seconds.
        Position now = {};
        const DWORD tick = GetTickCount();
        const PositionStatus status = ReadJimmyPosition(base, &now);
        if (status == PositionStatus::Good) {
            const bool moved = !previouslyAvailable ||
                (std::fabs(now.x - previous.x) + std::fabs(now.y - previous.y) +
                 std::fabs(now.z - previous.z)) > 0.2f;
            if (moved || tick - lastLog >= 10000) {
                char line[160] = {};
                sprintf_s(line, sizeof(line), "BullyCoop position: x=%.2f y=%.2f z=%.2f time_ms=%lu\r\n",
                          now.x, now.y, now.z, static_cast<unsigned long>(tick));
                WriteProbeLog(line);
                previous = now;
                lastLog = tick;
            }
            previouslyAvailable = true;
        } else {
            if (previouslyAvailable || status != previousStatus || tick - lastWait >= 15000) {
                char line[180] = {};
                sprintf_s(line, sizeof(line), "BullyCoop position: waiting: %s\r\n", StatusName(status));
                WriteProbeLog(line);
                lastWait = tick;
            }
            previouslyAvailable = false;
        }
        previousStatus = status;
    }
    // Unreachable during normal game lifetime.
    return 0;
}

static BOOL CALLBACK StartProbe(PINIT_ONCE, PVOID, PVOID*) {
    HMODULE module = GetModuleHandleA(nullptr);
    if (!module) return TRUE;
    // Pin this DLL: the background thread should never run code from an unloaded module.
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            reinterpret_cast<LPCSTR>(&StartProbe), &self)) return TRUE;
    HANDLE thread = CreateThread(nullptr, 0, PositionThread, module, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        WriteProbeLog("BullyCoop position: unable to start reader thread\r\n");
        FreeLibrary(self);
    }
    return TRUE;
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version,
                                               REFIID iid, LPVOID* out, LPUNKNOWN outer) {
    typedef HRESULT (WINAPI* NativeFn)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    FARPROC proc = GetNativeProc("DirectInput8Create");
    if (!proc) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    const HRESULT result = reinterpret_cast<NativeFn>(proc)(instance, version, iid, out, outer);
    if (SUCCEEDED(result)) {
        WriteProbeLog("BullyCoop probe: DirectInput8Create forwarded successfully\r\n");
        InitOnceExecuteOnce(&g_probe_init, StartProbe, nullptr, nullptr);
    }
    return result;
}

extern "C" HRESULT WINAPI DllCanUnloadNow() {
    typedef HRESULT (WINAPI* NativeFn)();
    FARPROC fn = GetNativeProc("DllCanUnloadNow");
    return fn ? reinterpret_cast<NativeFn>(fn)() : S_FALSE;
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* out) {
    typedef HRESULT (WINAPI* NativeFn)(REFCLSID, REFIID, LPVOID*);
    FARPROC fn = GetNativeProc("DllGetClassObject");
    return fn ? reinterpret_cast<NativeFn>(fn)(clsid, iid, out) : CLASS_E_CLASSNOTAVAILABLE;
}
extern "C" HRESULT WINAPI DllRegisterServer() {
    typedef HRESULT (WINAPI* NativeFn)();
    FARPROC fn = GetNativeProc("DllRegisterServer");
    return fn ? reinterpret_cast<NativeFn>(fn)() : E_NOTIMPL;
}
extern "C" HRESULT WINAPI DllUnregisterServer() {
    typedef HRESULT (WINAPI* NativeFn)();
    FARPROC fn = GetNativeProc("DllUnregisterServer");
    return fn ? reinterpret_cast<NativeFn>(fn)() : E_NOTIMPL;
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
