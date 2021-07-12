#pragma once

/*
	Se pueden controlar los dispositivos con c�digos de 32 bits.
	La macro CTL_CODE devuelve un c�digo acorde al estandar en este caso
	recibe 4 argumentos.
		El segundo campo FuncitonCode es el que indica la operaci�n a intentar realizar

	Por cada operaci�n a soportar se puede modificar el segundo par�mtro de la macro
	Si se incluye este archivo en el c�digo de usuario ser� m�s r�pido saber el c�digo a enviar
*/

// ocultar un proceso a trav�s del pid 
#define IOCTL_HIDE_PROCESS_BY_PID CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_WRITE_ACCESS)

