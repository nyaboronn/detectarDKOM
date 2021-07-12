#pragma once

/*
	Se pueden controlar los dispositivos con códigos de 32 bits.
	La macro CTL_CODE devuelve un código acorde al estandar en este caso
	recibe 4 argumentos.
		El segundo campo FuncitonCode es el que indica la operación a intentar realizar

	Por cada operación a soportar se puede modificar el segundo parémtro de la macro
	Si se incluye este archivo en el código de usuario será más rápido saber el código a enviar
*/

// ocultar un proceso a través del pid 
#define IOCTL_HIDE_PROCESS_BY_PID CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_WRITE_ACCESS)

