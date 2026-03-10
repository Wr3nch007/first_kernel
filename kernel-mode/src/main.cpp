
#include <ntifs.h>


extern "C" { //undocumented windows api function because this cannot be called with ntifs.h header file we need to declare it here
	NTKERNELAPI NTSTATUS IoCreateDriver( //IOcreate driver is used make compatable for KDMapper
		_In_opt_ PUNICODE_STRING DriverName,
		_In_ PDRIVER_INITIALIZE InitializationFunction
	);
	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress, PEPROCESS TargetProcess, 
											PVOID TargetAddress, SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T NumberOfBytesCopied);
	//MMCopyVirtualMemory used to implement read and write process memory 
}


void debug_print(PCSTR text) {
#ifndef DEBUG
	UNREFERENCED_PARAMETER(text);

#endif //DEBUG
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));


	
}


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
	};

	NTSTATUS create(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		UNREFERENCED_PARAMETER(irp);


		IoCompleteRequest(irp, IO_NO_INCREMENT); //this is used to complete the request and send the response back to user mode

		return irp->IoStatus.Status;
	}

	NTSTATUS close(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		UNREFERENCED_PARAMETER(irp);



		IoCompleteRequest(irp, IO_NO_INCREMENT); //

		return irp->IoStatus.Status;
	}

	NTSTATUS device_control(PDEVICE_OBJECT device_object, PIRP irp) { //when we give the argument like attach, read, write it will come here and stored in irp
		UNREFERENCED_PARAMETER(device_object);
		UNREFERENCED_PARAMETER(irp);

		debug_print("Device control called\n"); //used for our confirmation that this device_control is working

		NTSTATUS status = STATUS_UNSUCCESSFUL;

		PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp); //this is used to get the current stack location of the irp and this is used to get the parameters from the user mode

		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer); //this is used to get the request and parameters from the user mode

		if (stack_irp == nullptr || request == nullptr) {
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			debug_print("Invalid request\n");
			return status;
		}

		static PEPROCESS target_process = nullptr;

		const ULONG control_code = stack_irp->Parameters.DeviceIoControl.IoControlCode; //this is used to get the control code from the user mode and this is used to determine which operation we want to perform

		switch (control_code) { //this switch case is from the namespace that we created as codes and this is used to determine which operation we want to perform
		case codes::attach:
			status = PsLookupProcessByProcessId(request->pid, &target_process);
			break;

		case codes::read: //this will be the read logic and this is used to read the process memory
			if (target_process != nullptr)
				status = MmCopyVirtualMemory(PsGetCurrentProcess(), request->buffer, target_process, request->target,
					request->size, KernelMode, &request->return_size);

			break;




		case codes::write:
			if (target_process != nullptr)
				status = MmCopyVirtualMemory(target_process, request->target, PsGetCurrentProcess(), request->buffer,
					request->size, KernelMode, &request->return_size);
			break;


		default:
			debug_print("Invalid control code\n");
			break;

			irp->IoStatus.Status = status;
			irp->IoStatus.Information = sizeof(Request);

			IoCompleteRequest(irp, IO_NO_INCREMENT); //


			return status;
		}

	}
}

NTSTATUS driver_main(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path) {
	UNREFERENCED_PARAMETER(driver_object);
	UNREFERENCED_PARAMETER(registry_path);
	//these parameter removes the compiler warnings

	//creating a device object for the driver so that we can interact with it from user mode
	UNICODE_STRING device_name = {};
	RtlInitUnicodeString(&device_name, L"\\Device\\first_kernel"); //this creates a device object inside the kernel object manager with the name \Device\first_kernel and this is used to identify the device in the kernel

	PDEVICE_OBJECT device_object = nullptr;
	NTSTATUS status = IoCreateDevice(driver_object, 0, &device_name, FILE_DEVICE_UNKNOWN, 
										FILE_DEVICE_SECURE_OPEN, FALSE, &device_object); //this creates the device object. Like first user app create device object then it moves to driver

	if (status != STATUS_SUCCESS) {
		debug_print("Failed to create driver device\n");
		return status;
	}

	debug_print("Driver loaded successfully\n");


	//creating symbolic link to the device so that we can access it from user mode
	UNICODE_STRING symbolic_link = {};
	RtlInitUnicodeString(&symbolic_link, L"\\DosDevices\\first_kernel"); 
	//we need to create a symbolic link to the device so that we can access it from user mode
	
	status = IoCreateSymbolicLink(&symbolic_link, &device_name);
	if (status != STATUS_SUCCESS) {
		debug_print("Failed to create symbolic link\n"); //if we fail to create the symbolic link it will through this error msg
		return status;
	}

	debug_print("Symbolic link created successfully\n");

	SetFlag(device_object->Flags, DO_BUFFERED_IO);
	//this flag is used to send small amount of data from user-mode to kernel-mode

	//set the driver handlers to our function with our logic.
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver::create; //this is used to set the create function for the driver
	driver_object->MajorFunction[IRP_MJ_CLOSE] = driver::close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::device_control;
	//this is happening in MajorFunction28 when we hover it we can see it
	//when any events happens in the driver it will saves it in this arrayf

	ClearFlag(device_object->Flags, DO_DEVICE_INITIALIZING);

	debug_print("Driver initialized successfully\n");

	return STATUS_SUCCESS;

	

}


NTSTATUS DriverEntry() { //DriverEntry is the entry point of every kernel mode driver and this is where the driver starts executing when it is loaded into the kernel
	debug_print("Msg from Kernel!\n");

	//creating a driver object for the driver so that we can interact with it from user mode
	UNICODE_STRING driver_name = {};
	RtlInitUnicodeString(&driver_name, L"\\Driver\\first_kernel"); //creates a driver object inside the kernel object manager with the name \Driver\first_kernel and this is used to identify the driver in the kernel

	return IoCreateDriver(&driver_name, &driver_main); //IOCreateDriver is used to create a driver object name \driver\first_kernel and use driver_main as the entry point
}
