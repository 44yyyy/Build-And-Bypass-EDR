#include <windows.h>
#include <iostream>
#include <cstdint>

#pragma optimize("", off)

void* trampoline = nullptr;
typedef void (*target_t)(const char*);

void write_jump(uint8_t* from, uint8_t* to) { // Writes the jump instruction at our target to jump to our proxy
    from[0] = 0xE9; // JMP opcode
    *((int32_t*)(from + 1)) = to - from - 5; // Calculate relative offset and write it in the next 4 bytes
}

bool trampoline_hook(void* target, void* proxy, void** trampolineout, size_t len) {
    // Cast addresses to byte pointers
    uint8_t* target_bytes = (uint8_t*)target;
    uint8_t* proxy_bytes = (uint8_t*)proxy;

    // Allocate trampoline
    uint8_t* trampoline = (uint8_t*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!trampoline) {
        return false;
    }

    // Unlock target memory
    DWORD old;
    VirtualProtect(target_bytes, len, PAGE_EXECUTE_READWRITE, &old);

    // Setup trampoline (Copy 'len' bytes, write jump back to target + 'len')
    memcpy(trampoline, target_bytes, len);
    write_jump(trampoline + len, target_bytes + len);

    // Write hook (Jump from target to proxy)
    write_jump(target_bytes, proxy_bytes);

    // Restore memory and save trampoline address
    VirtualProtect(target_bytes, len, old, &old);
    *trampolineout = trampoline;

    return true;
}

void target(const char* message) {
    std::cout << "Target executing. Payload: " << message << "\n";
}

void proxy(const char* message) {
    std::cout << "\nInterception triggered.\n";

    if (std::string(message) == "Malicious Shellcode") {
        std::cout << "Malicious payload dropped: " << message << "\n";
        return;
    }

    std::cout << "Payload safe. Forwarding to trampoline: " << message << "\n";
    target_t original = (target_t)trampoline;
    original(message);
}

int main() {
    std::cout << "Simplified EDR Hooking Example.\n\n";

    target("Hi");

    if (trampoline_hook((void*)target, (void*)proxy, &trampoline, 8)) {
        std::cout << "\nHook installed successfully.\n";
    }

    target("Safe Data");
    target("Malicious Shellcode");

    return 0;
}