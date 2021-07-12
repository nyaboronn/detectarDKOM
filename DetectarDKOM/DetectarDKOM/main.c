
#include <Ntifs.h>
#include <ntddk.h>
#include <wdm.h>
#include <Ntifs.h>

// Pools
DWORD32* poolPids = NULL;
DWORD32* poolFlags = NULL;
// Variable para guardar el PID del explore.exe al iniciar el driver
DWORD32 _EXPLOREREXEPID = 0;
// Cuando es TRUE hace break al thread 
BOOLEAN _KILL = FALSE;
// Puntero al objeto thread
PVOID _THREADOBJECT;
DRIVER_INITIALIZE DriverEntry;

/*
	LISTADO DE OFFSETS
*/
#define OFFSET_ACTIVEPROCESSLINKS 0xe8/4
#define OFFSET_IMAGEFILENAME 0x6B
#define OFFSET_PROCESSPID 0x39

// Número de PIDS máximo a gestionar
#define POOLSIZE 8


/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;Función para obtener el PID dado el nombre de la imagen, recorre la lista
;;ActiveProcessLinks y compara por cada EPROCESS el parámetro de entrada.
;; PARAMETROS ;;
;;	file_name => nombre de imagen a buscar en la lista de procesos
;; RETURN ;;
;;	explorer_pid => null si explorer.exe no se encuentra
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/
NTSTATUS getPidByFileName(CHAR* file_name, DWORD32* explorer_pid)
{
	PEPROCESS first = PsInitialSystemProcess;
	PEPROCESS actual = first;
	PLIST_ENTRY list_aux = NULL;

	do {
		if (strcmp(file_name, (char*)((PVOID*)actual + OFFSET_IMAGEFILENAME)) == 0)
		{
			*explorer_pid = *((int*)actual + OFFSET_PROCESSPID);
			return STATUS_SUCCESS;
		}

		list_aux = (PLIST_ENTRY)((PVOID*)actual + OFFSET_ACTIVEPROCESSLINKS);

		// FLink apunta al siguiente PLIST_ENTRY, hay que restarle su offset para obtener el inicio del EPROCESS
		actual = (PEPROCESS)((PVOID*)list_aux->Flink - OFFSET_ACTIVEPROCESSLINKS);
	} while (actual != first);

	return STATUS_UNSUCCESSFUL;
}


/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Devuelve true si el DWORD32 es iguala al gun pid que
;; contienen los EPROCESS
;; PARAMETROS ;;
;; pid => pid a buscar
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/
BOOLEAN findPidInActiveLinks(DWORD32* pid)
{
	PEPROCESS first = PsInitialSystemProcess;
	PEPROCESS actual = first;
	PLIST_ENTRY list_aux = NULL;

	do {
		if (*((DWORD32*)actual + OFFSET_PROCESSPID) == *pid)
		{
			return TRUE;
		}

		list_aux = (PLIST_ENTRY)((PVOID*)actual + OFFSET_ACTIVEPROCESSLINKS);
		actual = (PEPROCESS)((PVOID*)list_aux->Flink - OFFSET_ACTIVEPROCESSLINKS);
	} while (actual != first);

	return FALSE;
}


/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Compara las pools con la lista doblemente enlazada de los EPROCESS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/
VOID buscarProcesosEscondidos()
{
	for (size_t i = 0; i < POOLSIZE; ++i)
	{
		if (*(poolFlags + i) == 1 && !findPidInActiveLinks(poolPids + i))
		{
			DbgPrint("[TAREA1][PROCESSHIDEN] %d ha sido escondido", *(poolPids + i));
			*(poolFlags + i) = 0;
			*(poolPids + i) = 0;
		}
	}
}


/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;Bucle infinito que busca procesos escondidos en cada interación, en cada
;;una se ejecuta un delay en interval milisegundos
;; PARAMETROS ;;
;;	Context => sin usar
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/
VOID ThreadCheckHiddenProcess(IN PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	LARGE_INTEGER interval;
	interval.QuadPart = -3 * 1000 * 1000 * 10;

	DbgPrint("[TAREA1][THREAD] : inicio bucle infinito");

	for (;;)
	{
		NTSTATUS st = KeDelayExecutionThread(KernelMode, FALSE, &interval);

		if (NT_SUCCESS(st))
		{
			DbgPrint("[TAREA1][THREAD] : delay terminado, buscando procesos escondidos...");
			buscarProcesosEscondidos();
		}

		if (_KILL == TRUE) {
			break;
		}
	}

	PsTerminateSystemThread(STATUS_SUCCESS);
}


/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;Función para añadir el PID, recorre la lista de flags hasta encontrar
;;una posición libre.
;; PARAMETROS ;;
;;	pid => pid a borrar
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/
int findFreePosition()
{
	for (size_t i = 0; i < POOLSIZE; ++i)
	{
		if (*(poolFlags + i) == 0) {
			return i;
		}
	}

	return -1;
}


/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;Función para añadir el PID, recorre la lista de flags hasta encontrar
;;una posición libre.
;; PARAMETROS ;;
;;	pid => pid a borrar
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/
VOID addPoolValue(DWORD32* pid)
{
	int libre = findFreePosition();

	if (libre > -1)
	{
		DbgPrint("[TAREA1][POOL] : pid guardado en la pos %d", libre);
		*(poolPids + libre) = *pid;
		*(poolFlags + libre) = 1; // 1 implica que está en uso
		return;
	}

	DbgPrint("[TAREA1][POOL][WARNING] : PID NO REGISTRADO, sin espacio para más procesos");
}


/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;Pone a 0 la posíción de memoria que contenga la DWORD32 dado por parámetro
;; y setea el flag 0 para indicar que está libre
;; PARAMETROS ;;
;;	pid => pid a borrar
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/
VOID removePoolValue(DWORD32* pid)
{
	for (size_t i = 0; i < POOLSIZE; ++i)
	{
		if (*(poolPids + i) == *pid)
		{
			*(poolPids + i) = 0;
			*(poolFlags + i) = 0;
			return;
		}
	}
	DbgPrint("[TAREA1][POOL] : pid no listado, no quedaría espacio en el registro");
}


VOID sCreateProcessNotifyRoutine(HANDLE ppid, HANDLE pid, BOOLEAN create)
{
	if ((DWORD32)ppid == _EXPLOREREXEPID)
	{
		DWORD32 id = (DWORD32)pid;
		DbgPrint("[TAREA1][NOTIFY] : pid %d es hijo del explorer", id);
		if (create)
		{
			DbgPrint("[TAREA1][NOTIFY] : añadiendo el proceso...");
			addPoolValue(&id);
		}
		else
		{
			DbgPrint("[TAREA1][NOTIFY] : quitando el proceso...");
			removePoolValue(&id);
		}
	}
}


VOID UnloadFunc(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING deviceUserModeName;
	RtlInitUnicodeString(&deviceUserModeName, L"\\??\\Tarea1");
	IoDeleteSymbolicLink(&deviceUserModeName);
	IoDeleteDevice(DriverObject->DeviceObject);

	// Remove el registro al callback de las notificaciones
	PsSetCreateProcessNotifyRoutine(sCreateProcessNotifyRoutine, TRUE);

	// Liberar las pools, comprobar null por si hay algún error a medias
	if (poolPids != NULL) {
		ExFreePoolWithTag(poolPids, 'pids');
	}
	if (poolFlags != NULL) {
		ExFreePoolWithTag(poolFlags, 'flag');
	}

	_KILL = TRUE; 	// Break al bucle infinito del thread

	KeWaitForSingleObject(_THREADOBJECT,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	ObDereferenceObject(_THREADOBJECT);

	DbgPrint("[TAREA1][UNLOAD] : Unloaded Tarea1!!");
}

NTSTATUS initializateData()
{
	size_t flagsSize = 8;
	poolPids = (DWORD32*)ExAllocatePoolWithTag(NonPagedPool, (sizeof(DWORD32) * flagsSize), 'pids');
	poolFlags = (DWORD32*)ExAllocatePoolWithTag(NonPagedPool, (sizeof(DWORD32) * flagsSize), 'flag');

	if (!poolPids || !poolFlags) {
		return STATUS_INSUFFICIENT_RESOURCES;
	};

	RtlZeroMemory(poolPids, (sizeof(DWORD32) * flagsSize));
	RtlZeroMemory(poolFlags, (sizeof(DWORD32) * flagsSize));

	DbgPrint("[TARE1][INIT] : poolPids address %p", poolPids);
	DbgPrint("[TARE1][INIT] : poolFlags address %p", poolFlags);

	if (getPidByFileName("explorer.exe", &_EXPLOREREXEPID) == STATUS_UNSUCCESSFUL) {
		DbgPrint("[TARE1][INIT] : explorer chungo");
		return STATUS_UNSUCCESSFUL;
	}

	DbgPrint("[TARE1][INIT] : explore.exe pid = %d", _EXPLOREREXEPID);

	/*
		Iniciar el thread que busca procesos escondidos
	*/
	VOID(*puntoEntradaAlThread)(PVOID) = ThreadCheckHiddenProcess;

	HANDLE threadHandle;

	NTSTATUS status = PsCreateSystemThread(
		&threadHandle,
		(ACCESS_MASK)0,
		NULL,
		0,
		NULL,
		puntoEntradaAlThread,
		poolPids
	);

	if (!NT_SUCCESS(status)) {
		DbgPrint("[TARE1][INIT] : no se ha podido crear el thread");
		return status;
	}

	/*
		Convertir de threadHadle a un puntero al threadobject
		Después cerrar el handle
	*/

	ObReferenceObjectByHandle(threadHandle,
		THREAD_ALL_ACCESS,
		NULL,
		KernelMode,
		&_THREADOBJECT,
		NULL);

	ZwClose(threadHandle);

	return STATUS_SUCCESS;
}

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

NTSTATUS DispatchIOCTL(PDEVICE_OBJECT Device, PIRP Irp)
{
	UNREFERENCED_PARAMETER(Device);
	NTSTATUS status;

	status = STATUS_UNSUCCESSFUL;

	DbgPrint("[TAREA1][INFO] : No hay IOCTL soportados");

	return SimpleComplete(status, Irp);
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT     DriverObject, _In_ PUNICODE_STRING    RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	UNICODE_STRING deviceName;
	UNICODE_STRING deviceUserModeName;
	PDEVICE_OBJECT device;
	NTSTATUS status;

	DriverObject->DriverUnload = UnloadFunc;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIOCTL;

	RtlInitUnicodeString(&deviceName, L"\\Device\\Tarea1");
	RtlInitUnicodeString(&deviceUserModeName, L"\\??\\Tarea1");

	status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &device);

	if (status != STATUS_SUCCESS) {
		DbgPrint("[TAREA1][ERROR] : There was a problem creating the device!!");
		return status;
	}

	status = IoCreateSymbolicLink(&deviceUserModeName, &deviceName);
	if (status != STATUS_SUCCESS) {
		DbgPrint("[TAREA1][ERROR] : There was a problem creating the symbolic Link!!");
		return status;
	}

	status = initializateData();
	if (status != STATUS_SUCCESS) {
		DbgPrint("[TAREA1][ERROR] : Error al reservar memoria para las listas de procesos!!");
		return status;
	}

	DbgPrint("[TAREA1][INFO] : driver cargado");

	PsSetCreateProcessNotifyRoutine(sCreateProcessNotifyRoutine, FALSE);

	return STATUS_SUCCESS;
}

