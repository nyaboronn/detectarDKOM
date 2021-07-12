# About

Detectar el ataque DKOM a procesos hijos del explorer.exe mediante un driver.

# Process Hide con DKOM 

Direct kernel object manipulation (DKOM) consiste en modificar estructuras de datos que usa el kernel de windows. Los procesos mostrados en el explorador de tareas es información que se encuentra en una lista doblemente enlazada donde cada nodo de la lista es un struct del tipo EPROCESS, los cuales contienen todas la información relacionada con un proceso. En el siguiente [enlace](https://www.codeproject.com/Articles/800404/Understanding-LIST-ENTRY-Lists-and-Its-Importance) se explica en detalle lo comentado.

La API de windows permite consultar la información de la lista desde modo usuario pero para modificarla hay que hacerlo desde el modo kernel, es decir, programar un driver (servicio al no estar asociado a ningún hardware) permitiendo recorrrerla sumando los offsets correspondientes.

# Compilar

Cada carpeta es un proyecto de Visual Studio:
* **ProcessHide**: driver que recive por un IOCTL el PID a ocultar.
* **ProcessHideUserCall**: programa modo usuario para llamar al driver ProcessHide, recive como entrada un string con el nombre del proceso a ocultar.
* **DetectarDKOM**: el driver que detecta el DKOM.

Tan solo es abrir los .sln y hacer build del proyecto para la arquitectura correspondiente. Los drivers solo se firma con la de testeo.

# Ejemplo de salida con notepad.exe

He usado la salida de debug para mostrar los avisos. Al iniciar el driver se indica los valores inicializados y avisa del inicio del thread que comprueba si algún EPROCESS se ha ocultado.

![driver_cargado](https://github.com/nyaboronn/detectarDKOM/blob/main/readmeResources/driver_cargado.png)

Al ejecutar notepad.exe el driver detecta que es hijo del explorer.exe y registra su PID para tenerlo controlado.

![proceso_creado](https://github.com/nyaboronn/detectarDKOM/blob/main/readmeResources/proceso_creado.png)

En el caso que un proceso registrado termina se borra su PID de la pool.

![proceso_terminado](https://github.com/nyaboronn/detectarDKOM/blob/main/readmeResources/proceso_terminado.png)

Con al notepad.exe escondido el thread termina el delay, comprueba la lista doblemente enlazada con la pool del driver. Hay un proceso en la pool que la lista no, por tanto el proceso ha sido escondido.

![dkom_detectado](https://github.com/nyaboronn/detectarDKOM/blob/main/readmeResources/dkom_detectado.png)

