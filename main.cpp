#include <pch.h>
#include <dwmapi.h>
#include <thread>
#include <future>
#include <unordered_set>
#include <memory>
#include <Pch.h>

struct Info {
    uint32_t index;
    uint32_t process_id;
    uint64_t dtb;
    uint64_t kernelAddr;
    std::string name;
};

uint64_t cbSize = 0x80000;

VOID cbAddFile(_Inout_ HANDLE h, _In_ LPSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo) {
    if (strcmp(uszName, "dtb.txt") == 0) {
        cbSize = cb;
    }
}

bool Memory::FixCr3() {
    PVMMDLL_MAP_MODULEENTRY module_entry;

    if (VMMDLL_Map_GetModuleFromNameU(this->vHandle, this->current_process.PID,
                                      (LPSTR)this->current_process.process_name.c_str(), &module_entry, nullptr)) {
        return true;
    }

    if (!VMMDLL_InitializePlugins(this->vHandle)) {
        LOG("[-] Failed to initialize VMMDLL plugins.\n");
        return false;
    }

    if (!WaitForProgressCompletion()) {
        return false;
    }

    if (!ListProcInfoFiles()) {
        return false;
    }

    return TryPatchDtb(module_entry);
}

bool Memory::WaitForProgressCompletion() {
    auto start_time = std::chrono::steady_clock::now();
    BYTE bytes[4] = {0};
    DWORD bytes_read = 0;
    
    while (true) {
        auto nt = VMMDLL_VfsReadW(this->vHandle, (LPWSTR)L"\\misc\\procinfo\\progress_percent.txt", bytes, 3, &bytes_read, 0);
        if (nt == VMMDLL_STATUS_SUCCESS && atoi((LPSTR)bytes) == 100) {
            return true;
        }

        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            LOG("[-] Timeout waiting for progress.\n");
            return false; 
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Memory::ListProcInfoFiles() {
    VMMDLL_VFS_FILELIST2 VfsFileList = {VMMDLL_VFS_FILELIST_VERSION, 0, 0, cbAddFile};
    if (!VMMDLL_VfsListU(this->vHandle, (LPSTR)"\\misc\\procinfo\\", &VfsFileList)) {
        LOG("[-] Failed to list files in procinfo.\n");
        return false;
    }
    return true;
}

bool Memory::TryPatchDtb(PVMMDLL_MAP_MODULEENTRY& module_entry) {
    auto bytes = std::make_unique<BYTE[]>(cbSize);
    DWORD bytes_read = 0;
    auto nt = VMMDLL_VfsReadW(this->vHandle, (LPWSTR)L"\\misc\\procinfo\\dtb.txt", bytes.get(), cbSize - 1, &bytes_read, 0);
    if (nt != VMMDLL_STATUS_SUCCESS) {
        LOG("[-] Failed to read DTB file.\n");
        return false;
    }

    std::vector<uint64_t> possible_dtbs;
    std::istringstream iss(std::string(reinterpret_cast<char*>(bytes.get()), bytes_read));
    std::string line;
    while (std::getline(iss, line)) {
        Info info = {};
        std::istringstream info_ss(line);
        if (info_ss >> std::hex >> info.index >> std::dec >> info.process_id >> std::hex >> info.dtb >> info.kernelAddr >> info.name) {
            if (info.process_id == 0 || this->current_process.process_name.find(info.name) != std::string::npos) {
                possible_dtbs.push_back(info.dtb);
            }
        }
    }

    for (const auto& dtb : possible_dtbs) {
        VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_PROCESS_DTB | this->current_process.PID, dtb);
        if (VMMDLL_Map_GetModuleFromNameU(this->vHandle, this->current_process.PID,
                                          (LPSTR)this->current_process.process_name.c_str(), &module_entry, nullptr)) {
            LOG("[+] Patched DTB.\n");
            return true;
        }
    }

    LOG("[-] Failed to patch DTB.\n");
    return false;
}

void main() {
    if (!TargetProcess.Init("r5apex.exe")) {
        printf("Failed to initialize process\n");
        return;
    }

    if (!TargetProcess.FixCr3()) {
        printf("Failed to fix CR3\n");
    }
}
