#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

static DWORD get_process_id(const wchar_t* process_name) {
	DWORD process_id = 0;

	HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snap_shot == INVALID_HANDLE_VALUE) {
		return process_id;

	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Process32FirstW(snap_shot, &entry) == TRUE){
		
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



int main() {
	std::cout << "exe testing\n";

	return 0;
}