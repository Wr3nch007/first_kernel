#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

static DWORD get_process_id(const wchar_t* process_name) {
	DWORD process_id = 0;

	HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snap_shot == INVALID_HANDLE_VALUE)
		return process_id;

	PROCESSENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Process32FirstW(snap_shot, &entry) == TRUE) {

		if (_wcsicmp(process_name, entry.szExeFile) == 0) {
			process_id = entry.th32ProcessID;
			else {
				while (Process32NextW(snap_shot, &entry) == TRUE) {
					if (_wcsicmp(process_name, entry.szExeFile) == 0) {
					process_id = entry.th32ProcessID;
						break;
					}
				}
			}

		}

		

	}
	CloseHandle(snap_shot);

	return process_id;

}


//create a snapshot of the modules in the process and iterate through them to find the base address of the module we want to inject into
static std::uintptr_t get_module_base (const DWORD pid, const wchar_t* module_name) {
	std::uintptr_t module_base = 0;

	HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32 , pid);
	if (snap_shot == INVALID_HANDLE_VALUE)
		return module_base;

	PROCESSENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Process32FirstW(snap_shot, &entry) == TRUE) {
		if (_wcsicmp(module_name, entry.szModule) != nullptr)
			module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
		else {
			while (Module32NextW(snap_shot, &entry) == TRUE) {
				if (wsstr(module_name, entry.szModule) != nullptr) {
					module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
					break;
				}
			}
		}

		}

	CloseHandle(snap_shot);

	return module_base;


	}
	
//the above mentioned program is created by cazz YT and \
// explained in his very first video need to watch it to understand this


//taken from the kernel driver
namespace driver {
	namespace codes {
		constexpr ULONG attach =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);
		//used to attach and this attach is just the name
		//method_buffered is used to send data to the driver and receive data from the driver in a buffer
		//until 0x000 - 0x7FF is used for system defined codes and 0x800 - 0xFFF is used for user defined codes
		//FILE_ANY_ACCESS is used to give storage permissions
		//ULONG is a kernel mode version of DWORD in user mode

		constexpr ULONG read =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);
		//used to read process memory
		constexpr ULONG write =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS);
		//used to write process memory

	}

	struct Request {
		HANDLE pid; //gets the current process id that we want to interact with

		PVOID target; //gets the target address to read or write this is from MMCopyVirtualMemory
		PVOID buffer; //gets the buffer address to read or write this is from MMCopyVirtualMemory

		SIZE_T size; //gets the size of the data
		SIZE_T return_size; //actual bytes copied

		//we can change the name of this handle and others but the size and order of this should be the same eg. when i hover HANDLE its size is showing 8 bytes

		//create a attach, read and write functions that we declared before as standalone function in usermode.
		bool attach_to_process(HANDLE driver_handle, const DWORD pid) {
			Request r;
			r.pid = reinterpret_cast<HANDLE>(pid);

			return DeviceIoControl(driver_handle, codes::attach, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);

		}

	};


int main() {
	std::cout << "exe testing\n";

	return 0;
}