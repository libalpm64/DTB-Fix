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

    // Is this mapped? Check, If true we don't need to do it again.
    if (VMMDLL_Map_GetModuleFromNameU(this->vHandle, this->current_process.PID,
        (LPSTR)this->current_process.process_name.c_str(), &module_entry, NULL)) {
        return true;
    }

    if (!VMMDLL_InitializePlugins(this->vHandle)) {
        LOG("[-] Failed VMMDLL_InitializePlugins call\n");
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    while (true) {
        BYTE bytes[4] = { 0 };
        DWORD bytes_read = 0;
        auto nt = VMMDLL_VfsReadW(this->vHandle, (LPWSTR)L"\\misc\\procinfo\\progress_percent.txt", bytes, 3, &bytes_read, 0);
        if (nt == VMMDLL_STATUS_SUCCESS && atoi((LPSTR)bytes) == 100) {
            break;
        }
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
            LOG("[-] Timeout waiting for progress\n");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    VMMDLL_VFS_FILELIST2 VfsFileList = { VMMDLL_VFS_FILELIST_VERSION, 0, 0, cbAddFile };
    if (!VMMDLL_VfsListU(this->vHandle, (LPSTR)"\\misc\\procinfo\\", &VfsFileList)) {
        return false;
    }

    std::vector<uint64_t> possible_dtbs;
    auto bytes = std::make_unique<BYTE[]>(cbSize);
    DWORD bytes_read = 0;
    auto nt = VMMDLL_VfsReadW(this->vHandle, (LPWSTR)L"\\misc\\procinfo\\dtb.txt", bytes.get(), cbSize - 1, &bytes_read, 0);
    if (nt != VMMDLL_STATUS_SUCCESS) {
        return false;
    }


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
            (LPSTR)this->current_process.process_name.c_str(), &module_entry, NULL)) {
            LOG("[+] Patched DTB\n");
            return true;
        }
    }

    LOG("[-] Failed to patch module\n");
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
