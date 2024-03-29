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
#include <fstream>

/** instancia est�tica */
FATInterface* FATInterface::_static_instance = NULL;

static const char* _MODULE_ = "[FAT]............";
#define _EXPR_	(_defdbg && !IS_ISR())



/** Constructor
     *  Crea el gestor del sistema FAT asociando un nombre
     *  @param partition_label Nombre del sistema de ficheros en partition_table
     *  @param path: path para utilizar fat
     *  @param num_files_max, numero maximo de archivos en la fat
     *  @param format: true o false, formatear la particion si error al montar
     */
FATInterface::FATInterface(const char *partition_label, const char *path, int num_files_max,bool format) :  _error(0) {
	s_wl_handle= WL_INVALID_HANDLE;
	_ready = false;
//	_mounted = false;

	//memcpy(_label,partition_label,strlen(partition_label));
	sprintf(_label,"%s",partition_label);
	sprintf(_path,"/%s",path);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Path: %s Label: %s",_path,_label);
	//memcpy(_path,path,strlen(path));
	_num_files_max = num_files_max;

	_defdbg = true;


	setLoggingLevel(ESP_LOG_INFO);
	if(mount(format)!= ESP_OK)
		return;
	_static_instance = this;
}
FATInterface::~FATInterface(){
	umount();
	_static_instance = NULL;
	_ready = false;
}

///** ready
// *  Chequea si el sistema de ficheros est� listo
// *  @return True (si montado) o False
// */
//bool FATInterface::ready() {return _ready;};

///** getName
// *  Obtiene el nombre del sistema de ficheros
// *  @return _name Nombre asignado
// */
//const char* FATInterface::getName() { return _name; }


void FATInterface::setLoggingLevel(esp_log_level_t level){
	esp_log_level_set(_MODULE_, level);
}
//static FATInterface* FATInterface::getStaticInstance(){
//	return _static_instance;
//}
/**
 * @brief  		Monta la particion fat , indicada por partition_label en el path indicado en path
 * @param[in]	partition_label: label de la particion (partition table)
 * @param[in]	path: path reaiz que se usar� para la particion
 * @return True: Handle abierto, False: Handle no abierto (error)
 */
int FATInterface::mount(bool format) {
	esp_err_t _err;
	esp_vfs_fat_mount_config_t mount_config;

	mount_config.max_files = _num_files_max;
	mount_config.format_if_mount_failed = format;
	mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;

	_err = esp_vfs_fat_spiflash_mount(_path, _label, &mount_config, &s_wl_handle);
	if(_err != ESP_OK){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "Error montando Fatfs path:%s , label:%s  %s",_path,_label,esp_err_to_name(_err));
		return _err;
	}
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Fatfs CREADO CORRECTAMENTE");
	_ready = true;
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
//	_mounted = false;
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
	char* fullpath = new char[strlen(filename) + strlen(_path) + 2]();
	MBED_ASSERT(fullpath);
	sprintf(fullpath, "%s/%s", _path, filename);
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Abriendo archivo %s", fullpath);
	_mtx.lock();
	fp = fopen(fullpath, opentype);
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Archivo fp=%x", (uint32_t)fp);
	_mtx.unlock();
	delete(fullpath);
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
 * @brief funcion unlink con proteccion mutex
 * @param[in]	filename: nombre del fichero con su directorio.
 * @return 		int resultad.
 */
int FATInterface::_unlink(const char *filename){
	int result = 0;
	char* fullpath = new char[strlen(filename) + strlen(_path) + 2]();
	MBED_ASSERT(fullpath);
	sprintf(fullpath, "%s/%s", _path, filename);
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Eliminando archivo %s", fullpath);
	_mtx.lock();
	result = unlink(fullpath);
	_mtx.unlock();
	delete(fullpath);
	return result;

}
/**
 * @brief		funcion fwrite con proteccion mutex
 * @param[in]	data: puntero con los datos a escribir
 * @param[in]	size: tama�o de cada elemento a escribir
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
 * @param[in]	size: tama�o de cada elemento a leer
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


//-----------------------------------------------------------------------------------------
/**
 * @brief		funcion fread con proteccion mutex
 * @param[in]	data: puntero del buffer a rellenar con los datos leidos
 * @param[in]	size: tama�o de cada elemento a leer
 * @param[in]	count: numero de elementos a leer
 * @param[in]	stream: puntero FILE del archivo a leer
 * @return 		size_t: bytes leidos
 */
size_t FATInterface::readLine(char* result, size_t max_len, FILE *stream){
	size_t s=0;
	_mtx.lock();
	do{
		int count = fread(&result[s],sizeof(char),1,stream);
		if(count==0){
			break;
		}
		s+=count;
	}while(result[s-1] != '\n' && s < max_len);
	result[s]=0;
	_mtx.unlock();
	return s;
}


//-----------------------------------------------------------------------------------------
size_t FATInterface::getLineCount(FILE *stream){
	size_t s=0;
	char result=0;
	int count = 0;
	_mtx.lock();
	do{
		count = fread(&result,sizeof(char),1,stream);
		if(count && result == '\n'){
			s++;
		}
	}while(count > 0);
	_mtx.unlock();
	return s;
}

//-----------------------------------------------------------------------------------------
//int FATInterface::listFolder(const char* folder){//, std::list<const char*> &file_list){
int FATInterface::listFolder(const char* folder, std::list<const char*> *file_list){

	int count = -1;
	char* txt = new char[strlen(_path)+1+strlen(folder)+1]();
	MBED_ASSERT(txt);
	sprintf(txt, "%s/%s", _path, folder);
	DIR* dir = opendir(txt);
	if(dir){
		count = 0;
		struct dirent* de = NULL;
		while((de = readdir(dir)) != NULL){
			if(de->d_type == DT_REG){
				count++;
				char* name = new char[strlen(de->d_name)+1]();
				MBED_ASSERT(name);
				memcpy(name,(char *)de->d_name,strlen(de->d_name));
				DEBUG_TRACE_D(_EXPR_, _MODULE_, "Archivo %s",name);
				file_list->push_back(name);
			}
		}
		closedir(dir);
	}
	else{
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "dir = null");
	}
	delete(txt);
	return count;
}

//-----------------------------------------------------------------------------------------
int FATInterface::createFolder(const char* folder){
	int res=0;
	char* txt = new char[strlen(_path)+1+strlen(folder)+1]();
	MBED_ASSERT(txt);
	sprintf(txt, "%s/%s", _path, folder);
	DIR* dir = opendir(txt);
	if(!dir){
		int res = mkdir(txt, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
	}
	delete(txt);
	return res;
}

//-----------------------------------------------------------------------------------------
int FATInterface::copyFile(const char* src_file, const char* dest_file, bool erase_src){
	if(!fileExists(src_file)){
		return -1;
	}
	char* stxt = new char[strlen(_path)+1+strlen(src_file)+1]();
	MBED_ASSERT(stxt);
	sprintf(stxt, "%s/%s", _path, src_file);
	char* dtxt = new char[strlen(_path)+1+strlen(dest_file)+1]();
	MBED_ASSERT(dtxt);
	sprintf(dtxt, "%s/%s", _path, dest_file);

	std::ifstream srce( stxt, std::ios::binary );
	std::ofstream dest( dtxt, std::ios::binary );
	dest << srce.rdbuf();
	delete(dtxt);
	delete(stxt);
	if(!erase_src)
		return 0;
	return eraseFile(src_file);
}


//-----------------------------------------------------------------------------------------
int FATInterface::renameFile(const char* src_file, const char* dest_file){
	if(!fileExists(src_file)){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "Archivo src no existe %s",src_file);
		return -1;
	}
	if(fileExists(dest_file)){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "Archivo dst no existe %s",dest_file);
		return -1;
	}
	char* stxt = new char[strlen(_path)+1+strlen(src_file)+1]();
	MBED_ASSERT(stxt);
	sprintf(stxt, "%s/%s", _path, src_file);
	char* dtxt = new char[strlen(_path)+1+strlen(dest_file)+1]();
	MBED_ASSERT(dtxt);
	sprintf(dtxt, "%s/%s", _path, dest_file);

	return rename(stxt, dtxt);
}

//-----------------------------------------------------------------------------------------
int FATInterface::eraseFile(const char* f){
	char* stxt = new char[strlen(_path)+1+strlen(f)+1]();
	MBED_ASSERT(stxt);
	sprintf(stxt, "%s/%s", _path, f);
	int res = remove(stxt);
	delete(stxt);
	return res;
}

//-----------------------------------------------------------------------------------------
bool FATInterface::fileExists(const char* f){
	FILE* ptr = open(f, "r");
	if(ptr){
		close(ptr);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------------------
bool FATInterface::format(){
	//formateamos la particion FAT
	bool res = true;
	_mtx.lock();
	esp_partition_iterator_t fat_ite = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
	if(fat_ite != NULL){
		const esp_partition_t* part = esp_partition_get(fat_ite);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Inicio Formateamos FAT!!!!!!!!")
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Type: %d", (uint32_t)part->type);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"SubType: %d", (uint32_t)part->subtype);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Address: 0x%x", part->address);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Size: 0x%x", part->size);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Label: %d", (uint8_t)part->label);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Encrypted: %d", (uint8_t)part->encrypted);

		esp_err_t err = esp_partition_erase_range(part,0, part->size);
		if(err != ESP_OK){
			DEBUG_TRACE_I(_EXPR_,_MODULE_,"Error formateando partition: %d",(int)err);
			res = false;
		}
		else{
			DEBUG_TRACE_I(_EXPR_,_MODULE_,"Fin Formateamos FAT!!!!!!!!");
			esp_partition_iterator_release(fat_ite);
		}
	}
	else{
		DEBUG_TRACE_E(_EXPR_,_MODULE_,"Particion FAT no encontrada!!!!");
		res = false;
	}
	_mtx.unlock();
	return res;
}
