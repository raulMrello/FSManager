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
#include <sys/stat.h>
#include <sys/types.h>

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
	if(mount(format)!= ESP_OK){
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "Salimosssss");
		
		//return;
	}
	_static_instance = this;
}
FATInterface::~FATInterface(){
	umount();
	#if ESP_PLATFORM == 1
	if(_worker_th != NULL){
		WorkerJob job(WorkerOp::Stop);
		_dispatchJob(job);
		// Asegura que el worker ya no está ejecutando código (evita carreras SMP)
		// (si por algún motivo no llegase a señalizarse, no bloqueamos el destructor para siempre)
		_worker_stopped.wait(2000);
		delete _worker_th;
		_worker_th = NULL;
		_worker_tid = NULL;
		_worker_ok = false;
	}
	#endif
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _mount_internal(format);
	}
	WorkerJob job(WorkerOp::Mount);
	job.flag = format;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _mount_internal(format);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_mount_internal(bool format){
    if (_ready) return ESP_OK;  // ya montado

    esp_vfs_fat_mount_config_t mount_config = {};
    mount_config.max_files = _num_files_max;
    mount_config.format_if_mount_failed = format;
    mount_config.allocation_unit_size = 4096; // o 0 para auto

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
        _path,   // p.ej. "/fat"
        _label,  // p.ej. "fatfs" en partitions.csv
        &mount_config,
        &s_wl_handle
    );
    if (err != ESP_OK) {
        DEBUG_TRACE_E(_EXPR_, _MODULE_, "Error montando Fatfs path:%s , label:%s  %s",
                      _path, _label, esp_err_to_name(err));
        return err;
    }

    DEBUG_TRACE_I(_EXPR_, _MODULE_, "FATFS montado correctamente en %s (label=%s)", _path, _label);
    _ready = true;
    return ESP_OK;
}



/** @brief		Desmonta la particion fat
 *
 */
int FATInterface::umount() {
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _umount_internal();
	}
	WorkerJob job(WorkerOp::Umount);
	_dispatchJob(job);
	return job.result_i;
	#else
	return _umount_internal();
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_umount_internal(){
	esp_err_t _err;

	_err = esp_vfs_fat_spiflash_unmount_rw_wl(_path, s_wl_handle);
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _open_internal(filename, opentype);
	}
	WorkerJob job(WorkerOp::Open);
	job.filename = filename;
	job.opentype = opentype;
	_dispatchJob(job);
	return job.result_fp;
	#else
	return _open_internal(filename, opentype);
	#endif

}


//-----------------------------------------------------------------------------------------
FILE* FATInterface::_open_internal(const char *filename,const char *opentype){
	FILE *fp = NULL;
	char* fullpath = new char[strlen(filename) + strlen(_path) + 2]();
	MBED_ASSERT(fullpath);
	sprintf(fullpath, "%s/%s", _path, filename);
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Abriendo archivo %s", fullpath);
	_mtx.lock();
	fp = fopen(fullpath, opentype);
	// fp es un puntero; casteado a uint32_t debe mostrarse con %lu
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Archivo fp=%lu", (uint32_t)fp);
	_mtx.unlock();
	delete[] fullpath;
	return fp;
}
/**
 * @brief 		funcion fclose con proteccion mutex
 * @param[in]	FILE *p: nombre archivo a cerrar
 * @return		int resultado
 */
int FATInterface::close(FILE *stream){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _close_internal(stream);
	}
	WorkerJob job(WorkerOp::Close);
	job.stream = stream;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _close_internal(stream);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_close_internal(FILE *stream){
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _unlink_internal(filename);
	}
	WorkerJob job(WorkerOp::Unlink);
	job.filename = filename;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _unlink_internal(filename);
	#endif

}


//-----------------------------------------------------------------------------------------
int FATInterface::_unlink_internal(const char *filename){
	int result = 0;
	char* fullpath = new char[strlen(filename) + strlen(_path) + 2]();
	MBED_ASSERT(fullpath);
	sprintf(fullpath, "%s/%s", _path, filename);
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Eliminando archivo %s", fullpath);
	_mtx.lock();
	result = unlink(fullpath);
	_mtx.unlock();
	delete[] fullpath;
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _write_internal(data, size, count, stream);
	}
	WorkerJob job(WorkerOp::Write);
	job.wdata = data;
	job.size = size;
	job.count = count;
	job.stream = stream;
	_dispatchJob(job);
	return job.result_sz;
	#else
	return _write_internal(data, size, count, stream);
	#endif
}


//-----------------------------------------------------------------------------------------
size_t FATInterface::_write_internal(const void *data,size_t size,size_t count,FILE*stream) {
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _read_internal(data, size, count, stream);
	}
	WorkerJob job(WorkerOp::Read);
	job.rdata = data;
	job.size = size;
	job.count = count;
	job.stream = stream;
	_dispatchJob(job);
	return job.result_sz;
	#else
	return _read_internal(data, size, count, stream);
	#endif
}


//-----------------------------------------------------------------------------------------
size_t FATInterface::_read_internal(void *data,size_t size, size_t count,FILE *stream){
	size_t s;
	_mtx.lock();
	s = fread(data,size,count,stream);
	_mtx.unlock();
	return s;
}


//-----------------------------------------------------------------------------------------
int FATInterface::seek(FILE* stream, long offset, int whence){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _seek_internal(stream, offset, whence);
	}
	WorkerJob job(WorkerOp::Seek);
	job.stream = stream;
	job.offset = offset;
	job.whence = whence;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _seek_internal(stream, offset, whence);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_seek_internal(FILE* stream, long offset, int whence){
	int res;
	_mtx.lock();
	res = fseek(stream, offset, whence);
	_mtx.unlock();
	return res;
}


//-----------------------------------------------------------------------------------------
long FATInterface::tell(FILE* stream){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _tell_internal(stream);
	}
	WorkerJob job(WorkerOp::Tell);
	job.stream = stream;
	_dispatchJob(job);
	return job.result_l;
	#else
	return _tell_internal(stream);
	#endif
}


//-----------------------------------------------------------------------------------------
long FATInterface::_tell_internal(FILE* stream){
	long res;
	_mtx.lock();
	res = ftell(stream);
	_mtx.unlock();
	return res;
}


//-----------------------------------------------------------------------------------------
int FATInterface::rewindFile(FILE* stream){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _rewind_internal(stream);
	}
	WorkerJob job(WorkerOp::Rewind);
	job.stream = stream;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _rewind_internal(stream);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_rewind_internal(FILE* stream){
	_mtx.lock();
	rewind(stream);
	_mtx.unlock();
	return 0;
}


//-----------------------------------------------------------------------------------------
int FATInterface::flush(FILE* stream){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _flush_internal(stream);
	}
	WorkerJob job(WorkerOp::Flush);
	job.stream = stream;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _flush_internal(stream);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_flush_internal(FILE* stream){
	int res;
	_mtx.lock();
	res = fflush(stream);
	_mtx.unlock();
	return res;
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _readLine_internal(result, max_len, stream);
	}
	WorkerJob job(WorkerOp::ReadLine);
	job.stream = stream;
	job.line_buf = result;
	job.line_max = max_len;
	_dispatchJob(job);
	return job.result_sz;
	#else
	return _readLine_internal(result, max_len, stream);
	#endif
}


//-----------------------------------------------------------------------------------------
size_t FATInterface::_readLine_internal(char* result, size_t max_len, FILE *stream){
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _getLineCount_internal(stream);
	}
	WorkerJob job(WorkerOp::GetLineCount);
	job.stream = stream;
	_dispatchJob(job);
	return job.result_sz;
	#else
	return _getLineCount_internal(stream);
	#endif
}


//-----------------------------------------------------------------------------------------
size_t FATInterface::_getLineCount_internal(FILE *stream){
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
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _listFolder_internal(folder, file_list);
	}
	WorkerJob job(WorkerOp::ListFolder);
	job.folder = folder;
	job.file_list = file_list;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _listFolder_internal(folder, file_list);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_listFolder_internal(const char* folder, std::list<const char*> *file_list){

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
	delete[] txt;
	return count;
}

//-----------------------------------------------------------------------------------------
int FATInterface::createFolder(const char* folder){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _createFolder_internal(folder);
	}
	WorkerJob job(WorkerOp::CreateFolder);
	job.folder = folder;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _createFolder_internal(folder);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_createFolder_internal(const char* folder){
	int res=0;
	char* txt = new char[strlen(_path)+1+strlen(folder)+1]();
	MBED_ASSERT(txt);
	sprintf(txt, "%s/%s", _path, folder);
	DIR* dir = opendir(txt);
	if(!dir){
		res = mkdir(txt, S_IRWXU | S_IRWXG | S_IRWXO);
	}
	delete[] txt;
	return res;
}

//-----------------------------------------------------------------------------------------
int FATInterface::copyFile(const char* src_file, const char* dest_file, bool erase_src){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _copyFile_internal(src_file, dest_file, erase_src);
	}
	WorkerJob job(WorkerOp::CopyFile);
	job.src = src_file;
	job.dest = dest_file;
	job.flag = erase_src;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _copyFile_internal(src_file, dest_file, erase_src);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_copyFile_internal(const char* src_file, const char* dest_file, bool erase_src){
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
	delete[] dtxt;
	delete[] stxt;
	if(!erase_src)
		return 0;
	return eraseFile(src_file);
}


//-----------------------------------------------------------------------------------------
int FATInterface::renameFile(const char* src_file, const char* dest_file){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _renameFile_internal(src_file, dest_file);
	}
	WorkerJob job(WorkerOp::RenameFile);
	job.src = src_file;
	job.dest = dest_file;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _renameFile_internal(src_file, dest_file);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_renameFile_internal(const char* src_file, const char* dest_file){
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
	int res = rename(stxt, dtxt);
	delete[] dtxt;
	delete[] stxt;
	return res;
}

//-----------------------------------------------------------------------------------------
int FATInterface::eraseFile(const char* f){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _eraseFile_internal(f);
	}
	WorkerJob job(WorkerOp::EraseFile);
	job.filename = f;
	_dispatchJob(job);
	return job.result_i;
	#else
	return _eraseFile_internal(f);
	#endif
}


//-----------------------------------------------------------------------------------------
int FATInterface::_eraseFile_internal(const char* f){
	char* stxt = new char[strlen(_path)+1+strlen(f)+1]();
	MBED_ASSERT(stxt);
	sprintf(stxt, "%s/%s", _path, f);
	int res = remove(stxt);
	delete[] stxt;
	return res;
}

//-----------------------------------------------------------------------------------------
bool FATInterface::fileExists(const char* f){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _fileExists_internal(f);
	}
	WorkerJob job(WorkerOp::FileExists);
	job.filename = f;
	_dispatchJob(job);
	return job.result_b;
	#else
	return _fileExists_internal(f);
	#endif
}


//-----------------------------------------------------------------------------------------
bool FATInterface::_fileExists_internal(const char* f){
	FILE* ptr = _open_internal(f, "r");
	if(ptr){
		_close_internal(ptr);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------------------
bool FATInterface::format(){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _format_internal();
	}
	WorkerJob job(WorkerOp::Format);
	_dispatchJob(job);
	return job.result_b;
	#else
	return _format_internal();
	#endif
}


//-----------------------------------------------------------------------------------------
bool FATInterface::_format_internal(){
	//formateamos la particion FAT
	bool res = true;
	_mtx.lock();
	esp_partition_iterator_t fat_ite = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
	if(fat_ite != NULL){
		const esp_partition_t* part = esp_partition_get(fat_ite);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Inicio Formateamos FAT!!!!!!!!")
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Type: %lu", (unsigned long)part->type);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"SubType: %lu", (unsigned long)part->subtype);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Address: 0x%08lx", (unsigned long)part->address);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Size: 0x%08lx", (unsigned long)part->size);
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Label: %s", part->label ? part->label : "<null>");
		DEBUG_TRACE_I(_EXPR_,_MODULE_,"Encrypted: %u", (unsigned)part->encrypted);

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


#if ESP_PLATFORM == 1
//------------------------------------------------------------------------------------
void FATInterface::_ensureWorker(){
	if(_worker_th != NULL){
		return;
	}
	_worker_ok = false;
	_worker_tid = NULL;
	_worker_th = new Thread(osPriorityNormal, 4096, NULL, "FATWorker");
	MBED_ASSERT(_worker_th);
	_worker_th->start(callback(this, &FATInterface::_workerTask));
	_worker_started.wait();
	_worker_ok = (_worker_tid != NULL);
}


//------------------------------------------------------------------------------------
bool FATInterface::_inWorkerContext() const{
	return (_worker_tid != NULL) && (Thread::gettid() == _worker_tid);
}


//------------------------------------------------------------------------------------
void FATInterface::_dispatchJob(WorkerJob& job){
	_ensureWorker();
	if(!_worker_ok){
		job.result_i = -1;
		job.result_b = false;
		job.result_sz = 0;
		job.result_l = -1;
		job.result_fp = NULL;
		job.done.release();
		return;
	}

	if(_inWorkerContext()){
		switch(job.op){
			case WorkerOp::Mount:
				job.result_i = _mount_internal(job.flag);
				break;
			case WorkerOp::Umount:
				job.result_i = _umount_internal();
				break;
			case WorkerOp::Open:
				job.result_fp = _open_internal(job.filename, job.opentype);
				break;
			case WorkerOp::Close:
				job.result_i = _close_internal(job.stream);
				break;
			case WorkerOp::Unlink:
				job.result_i = _unlink_internal(job.filename);
				break;
			case WorkerOp::Write:
				job.result_sz = _write_internal(job.wdata, job.size, job.count, job.stream);
				break;
			case WorkerOp::Read:
				job.result_sz = _read_internal(job.rdata, job.size, job.count, job.stream);
				break;
			case WorkerOp::Seek:
				job.result_i = _seek_internal(job.stream, job.offset, job.whence);
				break;
			case WorkerOp::Tell:
				job.result_l = _tell_internal(job.stream);
				break;
			case WorkerOp::Rewind:
				job.result_i = _rewind_internal(job.stream);
				break;
			case WorkerOp::Flush:
				job.result_i = _flush_internal(job.stream);
				break;
			case WorkerOp::ReadLine:
				job.result_sz = _readLine_internal(job.line_buf, job.line_max, job.stream);
				break;
			case WorkerOp::GetLineCount:
				job.result_sz = _getLineCount_internal(job.stream);
				break;
			case WorkerOp::ListFolder:
				job.result_i = _listFolder_internal(job.folder, job.file_list);
				break;
			case WorkerOp::CreateFolder:
				job.result_i = _createFolder_internal(job.folder);
				break;
			case WorkerOp::CopyFile:
				job.result_i = _copyFile_internal(job.src, job.dest, job.flag);
				break;
			case WorkerOp::RenameFile:
				job.result_i = _renameFile_internal(job.src, job.dest);
				break;
			case WorkerOp::EraseFile:
				job.result_i = _eraseFile_internal(job.filename);
				break;
			case WorkerOp::FileExists:
				job.result_b = _fileExists_internal(job.filename);
				break;
			case WorkerOp::Format:
				job.result_b = _format_internal();
				break;
			case WorkerOp::Stop:
				job.result_b = true;
				break;
		}
		job.done.release();
		return;
	}

	if(_worker_queue.put(&job, osWaitForever) != osOK){
		job.result_i = -1;
		job.result_b = false;
		job.result_sz = 0;
		job.result_l = -1;
		job.result_fp = NULL;
		job.done.release();
		return;
	}
	job.done.wait(osWaitForever);
}


//------------------------------------------------------------------------------------
void FATInterface::_workerTask(){
	_worker_tid = Thread::gettid();
	_worker_started.release();
	for(;;){
		osEvent ev = _worker_queue.get(osWaitForever);
		if(ev.status != osEventMessage){
			continue;
		}
		WorkerJob* job = (WorkerJob*)ev.value.p;
		if(job == NULL){
			continue;
		}
		if(job->op == WorkerOp::Stop){
			job->result_b = true;
			job->done.release();
			// Señalizamos que el worker está parado y nos suspendemos hasta que nos eliminen.
			_worker_stopped.release();
			vTaskSuspend(NULL);
			continue;
		}
		_dispatchJob(*job);
	}
}
#endif
