#pragma once

// External process memory access for cs2.exe.
// Plain OpenProcess + ReadProcessMemory/WriteProcessMemory. No injection,
// no thread creation in the target, no module loading.

#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

class Memory {
public:
    Memory() = default;
    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    ~Memory() {
        if (process_ && process_ != INVALID_HANDLE_VALUE)
            CloseHandle(process_);
    }

    // Finds the process by image name and opens it with the rights needed
    // for read/write/user-mode allocation. Same-user processes can be opened
    // without elevation as long as the target is not elevated/protected.
    bool Attach(const std::wstring& imageName) {
        DWORD pid = FindProcessId(imageName);
        if (!pid)
            return false;

        HANDLE h = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            FALSE, pid);
        if (!h)
            return false;

        if (process_ && process_ != INVALID_HANDLE_VALUE)
            CloseHandle(process_);
        process_ = h;
        pid_ = pid;
        return true;
    }

    bool Attached() const { return process_ && process_ != INVALID_HANDLE_VALUE; }
    DWORD Pid() const { return pid_; }
    HANDLE Handle() const { return process_; }

    // Base address of a loaded module (e.g. L"client.dll").
    uintptr_t ModuleBase(const std::wstring& moduleName) const {
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
        if (snap == INVALID_HANDLE_VALUE)
            return 0;

        uintptr_t base = 0;
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, moduleName.c_str()) == 0) {
                    base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return base;
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) const {
        SIZE_T got = 0;
        return ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(address), buffer, size, &got) &&
               got == size;
    }

    bool WriteRaw(uintptr_t address, const void* buffer, size_t size) const {
        SIZE_T put = 0;
        return WriteProcessMemory(process_, reinterpret_cast<LPVOID>(address), buffer, size, &put) &&
               put == size;
    }

    template <typename T>
    T Read(uintptr_t address) const {
        T value{};
        ReadRaw(address, &value, sizeof(T));
        return value;
    }

    template <typename T>
    bool Write(uintptr_t address, const T& value) const {
        return WriteRaw(address, &value, sizeof(T));
    }

    // Remote scratch memory for growing attribute vectors. Freed on Free()
    // or process detach. The game never owns these pages; see docs for the
    // leak/lifetime trade-off.
    uintptr_t Alloc(size_t size) {
        void* p = VirtualAllocEx(process_, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        return reinterpret_cast<uintptr_t>(p);
    }

    bool Free(uintptr_t address) {
        return VirtualFreeEx(process_, reinterpret_cast<LPVOID>(address), 0, MEM_RELEASE) != 0;
    }

private:
    static DWORD FindProcessId(const std::wstring& imageName) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return 0;

        DWORD pid = 0;
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, imageName.c_str()) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }

    HANDLE process_ = nullptr;
    DWORD pid_ = 0;
};
