#include <windows.h>
#include <iostream>

#pragma optimize("", off)

typedef int (WINAPI *MessageBoxA_t)(HWND, LPCSTR, LPCSTR, UINT); // MessageBoxA definition
MessageBoxA_t target_MessageBoxA = nullptr;
void* trampoline = nullptr;

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

int WINAPI proxy_MessageBoxA(HWND hwnd, LPCSTR pointer, LPCSTR caption, UINT type) {
    std::cout << "\nIntercepted OS call to MessageBoxA.\n";
        
    std::string text(pointer);
    if (text == "Malicious Payload") {
        std::cout << "Malicious payload detected. Blocking popup: " << pointer << "\n";
        return 0;
    }

    std::cout << "Payload safe. Allowing popup: " << pointer << "\n";
    target_MessageBoxA = (MessageBoxA_t)trampoline; // Cast trampoline back to function definition and execute
    return target_MessageBoxA(hwnd, pointer, caption, type);
}

void execute_bypass() {
	// Get adr of hooked api
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) {
        std::cout << "Bypass failed: Could not handle user32.dll\n";
        return;
    }

    uint8_t* hooked_api = (uint8_t*)GetProcAddress(user32, "MessageBoxA");
    if (!hooked_api) {
        std::cout << "Bypass failed: Could not find MessageBoxA\n";
        return;
    }

    // Allocate executable memory for our bypass trampoline
    uint8_t* bye_trampoline = (uint8_t*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!bye_trampoline) {
        std::cout << "Bypass failed: Could not allocate memory for trampoline\n";
        return;
    }

	// Microsoft hotpatch instructions
    bye_trampoline[0] = 0x8B; // mov edi, edi
    bye_trampoline[1] = 0xFF;
    bye_trampoline[2] = 0x55; // push ebp
    bye_trampoline[3] = 0x8B; // mov ebp, esp
    bye_trampoline[4] = 0xEC;

    // JMP to route execution back to the original api
    bye_trampoline[5] = 0xE9; // JMP opcode

    // Jump exactly to byte 6 (hooked_api + 5)
    uint8_t* destination = hooked_api + 5;
    uint8_t* source = bye_trampoline + 5;
    *((int32_t*)(bye_trampoline + 6)) = destination - source - 5;

    // Cast bypass trampoline into a callable function
    typedef int (WINAPI* MessageBoxA_t)(HWND, LPCSTR, LPCSTR, UINT);
    MessageBoxA_t sneaky_MessageBox = (MessageBoxA_t)bye_trampoline;

    // Execute
    std::cout << "\nExecuting bypass...\n";
    sneaky_MessageBox(NULL, "Malicious Payload", "Bypassed!", MB_OK);
}

int main() {
	HMODULE user32 = GetModuleHandleA("user32.dll"); // Get adr of MessageBoxA from user32.dll

    if (user32 == NULL) {
        std::cout << "Failed to find user32.dll\n";
        return 1;
    }

    void* target_adr = (void*)GetProcAddress(user32, "MessageBoxA");

    if (!target_adr) {
        std::cout << "Failed to find API address.\n";
        return 1;
    }

    std::cout << "Target API found at: " << target_adr << "\n";

    MessageBoxA(NULL, "Hello World", "Test 1", MB_OK); // Before hook

    size_t api_length = 5;
    if (trampoline_hook(target_adr, (void*)proxy_MessageBoxA, &trampoline, api_length)) { // Hook installation
        std::cout << "\nOS Hook Installed Successfully.\n";
    }

    MessageBoxA(NULL, "Safe Data", "Test 2", MB_OK); // Allow 
    MessageBoxA(NULL, "Malicious Payload", "Test 3", MB_OK); // Deny

	execute_bypass(); // Bypass hook

    return 0;
}