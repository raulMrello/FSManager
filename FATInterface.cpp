/*
 * FATInterface.cpp
 *
 *  Created on: Nov 2019
 *      Author: LJM
 *
 *	FATInterface provides FAT portable interface in C++
 *
 */
#include "FATInterface.h"

/** instancia estática */
FATInterface* FATInterface::_static_instance = NULL;

static const char* _MODULE_ = "[FAT]............";
#define _EXPR_	(_defdbg && !IS_ISR())



/** Constructor
     *  Crea el gestor del sistema FAT asociando un nombre
     *  @param name Nombre del sistema de ficheros
     */
FATInterface::FATInterface(const char *partition_label, const char *path, int num_files_max) :  _error(0) {
	s_wl_handle= WL_INVALID_HANDLE;
	_ready = false;
	//_mounted = false;

	//memcpy(_label,partition_label,strlen(partition_label));
	sprintf(_label,"%s",partition_label);
	sprintf(_path,"/%s",path);
	//memcpy(_path,path,strlen(path));
	_num_files_max = num_files_max;

	_defdbg = true;
	if(mount()!= ESP_OK)
		return;
	_ready = true;
	_static_instance = this;

}
FATInterface::~FATInterface(){
	_static_instance = NULL;
}

///** ready
// *  Chequea si el sistema de ficheros está listo
// *  @return True (si montado) o False
// */
//bool FATInterface::ready() {return _ready;};

///** getName
// *  Obtiene el nombre del sistema de ficheros
// *  @return _name Nombre asignado
// */
//const char* FATInterface::getName() { return _name; }

//static FATInterface* FATInterface::getStaticInstance(){
//	return _static_instance;
//}
/**
 * @brief  		Monta la particion fat , indicada por partition_label en el path indicado en path
 * @param[in]	partition_label: label de la particion (partition table)
 * @param[in]	path: path reaiz que se usará para la particion
 * @return True: Handle abierto, False: Handle no abierto (error)
 */
int FATInterface::mount() {
	esp_err_t _err;
	esp_vfs_fat_mount_config_t mount_config;

	mount_config.max_files = _num_files_max;
	mount_config.format_if_mount_failed = true;
	mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

	_err = esp_vfs_fat_spiflash_mount(_path, _label, &mount_config, &s_wl_handle);
	if(_err != ESP_OK){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "Error montando Fatfs %s",esp_err_to_name(_err));
		return _err;
	}
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Fatfs CREADO CORRECTAMENTE");
	//_mounted = true;
	return _err;
}


/** @brief		Desmonta la particion fat
 *
 */
int FATInterface::umount() {
	esp_err_t _err;

	_err = esp_vfs_fat_spiflash_unmount(_path, s_wl_handle);
	if(_err != ESP_OK){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "OTHER ERROR %s",esp_err_to_name(_err));
		return _err;
	}
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Fatfs Desmontado correctamente");
	//_mounted = false;
	return _err;
}
/**
 * @brief funcion fopen con proteccion mutex
 * @param[in]	filename: nombre del archivo a abrir
 * @param[in]	opentype: w,r,w+,r+,wb...
 * @return 		puntero al fichero abierto, si NULL error.
 */
FILE * FATInterface::open(const char *filename,const char *opentype){
	FILE *fp = NULL;
	_mtx.lock();
	fp = fopen(filename, opentype);
	_mtx.unlock();
	return fp;

}
/**
 * @brief 		funcion fclose con proteccion mutex
 * @param[in]	FILE *p: nombre archivo a cerrar
 * @return		int resultado
 */
int FATInterface::close(FILE *stream){
	int res = 0;
	_mtx.lock();
	res = fclose(stream);
	_mtx.unlock();
	return res;
}
/**
 * @brief		funcion fwrite con proteccion mutex
 * @param[in]	data: puntero con los datos a escribir
 * @param[in]	size: tamaño de cada elemento a escribir
 * @param[in]	count: numero de elementos a escribir
 * @param[in]	stream: puntero FILE del archivo a escribir
 * @return 		size_t: bytes escritos
 */
size_t FATInterface::write(const void *data,size_t size,size_t count,FILE*stream) {
	size_t s;
	_mtx.lock();
	s = fwrite(data,size,count,stream);
	_mtx.unlock();
	return s;
}
/**
 * @brief		funcion fread con proteccion mutex
 * @param[in]	data: puntero del buffer a rellenar con los datos leidos
 * @param[in]	size: tamaño de cada elemento a leer
 * @param[in]	count: numero de elementos a leer
 * @param[in]	stream: puntero FILE del archivo a leer
 * @return 		size_t: bytes leidos
 */
size_t FATInterface::read(void *data,size_t size, size_t count,FILE *stream){
	size_t s;
	_mtx.lock();
	s = fread(data,size,count,stream);
	_mtx.unlock();
	return s;
}
