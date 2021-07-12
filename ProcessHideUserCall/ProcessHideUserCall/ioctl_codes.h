#pragma once

// ocultar un proceso a través del pid 
#define IOCTL_HIDE_PROCESS_BY_PID CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_WRITE_ACCESS)

