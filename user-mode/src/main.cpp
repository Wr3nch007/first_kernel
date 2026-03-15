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

		}
			else {
				while (Process32NextW(snap_shot, &entry) == TRUE) {
					if (_wcsicmp(process_name, entry.szExeFile) == 0) {
					process_id = entry.th32ProcessID;
						break;
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

	MODULEENTRY32W entry = {};
	entry.dwSize = sizeof(entry);

	if (Module32FirstW(snap_shot, &entry) == TRUE) {
		do {
			if (_wcsicmp(module_name, entry.szModule) == 0) {
				module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
				break;
			}
		} while (Module32NextW(snap_shot, &entry) == TRUE);
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
		constexpr ULONG read =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);
		constexpr ULONG write =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS);
	}

	struct Request {
		HANDLE pid; //gets the current process id that we want to interact with
		PVOID target; //gets the target address to read or write this is from MMCopyVirtualMemory
		PVOID buffer; //gets the buffer address to read or write this is from MMCopyVirtualMemory
		SIZE_T size; //gets the size of the data
		SIZE_T return_size; //actual bytes copied

		// NOTE: helper functions were moved out of this struct (they were originally defined here).
	};

	// Moved helper functions out of Request so they can be called as driver::attach_to_process / driver::read_memory / driver::write_memory.
	static bool attach_to_process(HANDLE driver_handle, const DWORD pid) { // changed: moved out of Request (was member function)
		Request r = {};
		r.pid = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)); // changed: safer cast (was 'reinterpret_cast<HANDLE>(pid)')
		BOOL ok = DeviceIoControl(driver_handle, codes::attach, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr); //this DeviceIoControl will call the kernel driver Device_Control and display "Device control called" msg.
		return ok == TRUE; // changed: return boolean success (was returning DeviceIoControl result directly)
	}

	template <class T>
	static T read_memory(HANDLE driver_handle, const std::uintptr_t addr) { // changed: moved out of Request and fixed return (was member returning DeviceIoControl result)
		T temp = {};
		Request r = {};
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = &temp; // driver will put the result from MmCopyVirtualMemory into temp
		r.size = sizeof(T);

		BOOL ok = DeviceIoControl(driver_handle, codes::read, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
		(void)ok;
		return temp; // changed: return the filled buffer (was an unreachable return after DeviceIoControl)
	}

	template <class T>
	static bool write_memory(HANDLE driver_handle, const std::uintptr_t addr, const T& value) { // changed: moved out of Request and return bool (was void member)
		Request r = {};
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = const_cast<PVOID>(reinterpret_cast<const void*>(&value)); // changed: provide buffer to driver (was &value inside member, but function moved)
		r.size = sizeof(T);
		BOOL ok = DeviceIoControl(driver_handle, codes::write, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr); // changed: pass output buffer as &r for METHOD_BUFFERED
		return ok == TRUE;
	}

} // namespace driver


//call the notepad for testing
int main() {
	const DWORD pid = get_process_id(L"notepad.exe"); //this will search for notepad

	if (pid == 0) {
		std::cout << "notepad not found\n";
		std::cin.get();
		return 1;
	}

	const HANDLE driver = CreateFile(L"\\\\.\\first_kernel", GENERIC_READ | GENERIC_WRITE, 0, nullptr, 
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); // changed: added GENERIC_WRITE (was GENERIC_READ)

	if (driver == INVALID_HANDLE_VALUE) {
		std::cout << "failed to open driver\n";
		std::cin.get();
		return 1;
	}

	if (driver::attach_to_process(driver, pid) == true)
		std::cout << "{+} Attachment successful \n";
	else
		std::cout << "[-] Attachment failed\n";

	CloseHandle(driver);

	std::cin.get();

	return 0;
}
