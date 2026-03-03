/*
 * FSManager.cpp
 *
 *  Created on: Sep 2017
 *      Author: raulMrello
 */

#include "FSManager.h"
#if ESP_PLATFORM == 1
#include "nvs.h"
#include "nvs_flash.h"
#endif

//------------------------------------------------------------------------------------
//--- STATIC TYPES ------------------------------------------------------------------
//------------------------------------------------------------------------------------

FSManager* FSManager::_static_instance = NULL;

//------------------------------------------------------------------------------------
//--- PRIVATE TYPES ------------------------------------------------------------------
//------------------------------------------------------------------------------------

/** Macro para imprimir trazas de depuraci�n, siempre que se haya configurado un objeto
 *	Logger v�lido (ej: _debug)
 */
static const char* _MODULE_ = "[FS]............";
#define _EXPR_	(_defdbg && !IS_ISR())



//------------------------------------------------------------------------------------
//-- PUBLIC METHODS IMPLEMENTATION ---------------------------------------------------
//------------------------------------------------------------------------------------



//------------------------------------------------------------------------------------
FSManager::FSManager(const char *name, PinName32 mosi, PinName32 miso, PinName32 sclk, PinName32 csel, int freq, bool defdbg) : NVSInterface(name) {
    #if ESP_PLATFORM == 1
	_ready = false;
	_defdbg = defdbg;
	_handle = 0;
	_open_refcount = 0;
	_ensureWorker();
	init();
    #elif __MBED__ == 1
    //TODO
    #warning TODO FSManager::FSManager()
    #endif
	_static_instance = this;
}


//------------------------------------------------------------------------------------
FSManager::~FSManager(){
	#if ESP_PLATFORM == 1
	// detiene worker si existe
	if(_worker_th != NULL){
		WorkerJob job(WorkerOp::Stop);
		_dispatchJob(job);
		delete _worker_th;
		_worker_th = NULL;
	}
	// cierra handle si quedara abierto
	if(_handle){
		nvs_close(_handle);
		_handle = 0;
	}
	_worker_tid = NULL;
	_worker_ok = false;
	#endif
	_static_instance = NULL;
}


//------------------------------------------------------------------------------------
void FSManager::_ensureWorker(){
	#if ESP_PLATFORM == 1
	if(_worker_th != NULL){
		return;
	}
	_worker_ok = false;
	_worker_tid = NULL;
	_worker_th = new Thread(osPriorityNormal, 4096, NULL, "FSWorker");
	MBED_ASSERT(_worker_th);
	_worker_th->start(callback(this, &FSManager::_workerTask));
	_worker_started.wait();
	_worker_ok = (_worker_tid != NULL);
	#endif
}


//------------------------------------------------------------------------------------
bool FSManager::_inWorkerContext() const{
	#if ESP_PLATFORM == 1
	return (_worker_tid != NULL) && (Thread::gettid() == _worker_tid);
	#else
	return false;
	#endif
}


//------------------------------------------------------------------------------------
void FSManager::_dispatchJob(WorkerJob& job){
	#if ESP_PLATFORM == 1
	_ensureWorker();
	if(!_worker_ok){
		job.result_i = (int)ESP_FAIL;
		job.result_b = false;
		job.done.release();
		return;
	}
	if(_inWorkerContext()){
		// Si ya estamos en el contexto del worker, procesa sin cola
		switch(job.op){
			case WorkerOp::Init:
				job.result_i = _init_internal();
				break;
			case WorkerOp::Open:
				job.result_b = _open_internal();
				break;
			case WorkerOp::Close:
				_close_internal(false);
				job.result_b = true;
				break;
			case WorkerOp::Save:
				job.result_i = _save_internal(job.data_id, job.data, job.size, job.type);
				break;
			case WorkerOp::Restore:
				job.result_i = _restore_internal(job.data_id, job.data, job.size, job.type);
				break;
			case WorkerOp::CheckKey:
				job.result_b = _checkKey_internal(job.data_id);
				break;
			case WorkerOp::RemoveKey:
				job.result_i = _removeKey_internal(job.data_id);
				break;
			case WorkerOp::ErasePartition:
				job.result_b = _erase_internal();
				break;
			case WorkerOp::ListKeys:
				_list_nvs_keys_internal();
				job.result_b = true;
				break;
			case WorkerOp::EraseKeyList:
				if(job.keys_to_manage != NULL){
					_eraseKeyList_internal(*job.keys_to_manage, job.delete_only_these);
					job.result_b = true;
				}else{
					job.result_b = false;
				}
				break;
			case WorkerOp::Stop:
				job.result_b = true;
				break;
		}
		job.done.release();
		return;
	}

	// Encola y espera
	if(_worker_queue.put(&job, osWaitForever) != osOK){
		job.result_i = (int)ESP_FAIL;
		job.result_b = false;
		job.done.release();
		return;
	}
	job.done.wait(osWaitForever);
	#endif
}


//------------------------------------------------------------------------------------
void FSManager::_workerTask(){
	#if ESP_PLATFORM == 1
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
			break;
		}
		_dispatchJob(*job);
	}
	#endif
}


//------------------------------------------------------------------------------------
int FSManager::init() {
    #if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _init_internal();
	}
	WorkerJob job(WorkerOp::Init);
	_dispatchJob(job);
	return job.result_i;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::init()
    return -1;
    #endif
}


//------------------------------------------------------------------------------------
int FSManager::_init_internal(){
	#if ESP_PLATFORM == 1
	_ready = false;
	// Initialize NVS and the default partition
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// NVS partition was truncated and needs to be erased
		if(nvs_flash_erase() != ESP_OK)
			return ESP_FAIL;
		err = nvs_flash_init();
	}
	if(err != ESP_OK){
		return ESP_FAIL;
	}

	err = nvs_flash_init_partition(DEFAULT_NVSInterface_Partition);
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// NVS partition was truncated and needs to be erased
		if(nvs_flash_erase_partition(DEFAULT_NVSInterface_Partition) != ESP_OK)
			return ESP_FAIL;
		err = nvs_flash_init_partition(DEFAULT_NVSInterface_Partition);
	}
	if(err != ESP_OK){
		return ESP_FAIL;
	}

	// (Re)Open handle and keep it open for worker lifetime
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Chequeando sistema NVS ");
	nvs_handle hnd;
	err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &hnd);
	if (err != ESP_OK) {
		_handle = 0;
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN [%d]. No se puede abrir el sistema NVS", err);
		return err;
	}
	if(_handle){
		nvs_close(_handle);
	}
	_handle = hnd;
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS OK!");
	_ready = true;
	return err;
	#else
	return -1;
	#endif
}


//------------------------------------------------------------------------------------
bool FSManager::open(){
    #if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _open_internal();
	}
	WorkerJob job(WorkerOp::Open);
	_dispatchJob(job);
	return job.result_b;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::open()
    return false;
    #endif
}


//------------------------------------------------------------------------------------
bool FSManager::_open_internal(){
	#if ESP_PLATFORM == 1
	// Mantiene compatibilidad con la API original: open/close pueden agrupar operaciones.
	// Con worker dedicado ya no es imprescindible, pero mantenemos un refcount.
	_open_refcount++;
	if(_handle){
		return true;
	}
	nvs_handle hnd;
	esp_err_t err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &hnd);
	if(err != ESP_OK){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN [%d] al abrir el sistema NVS", err);
		_open_refcount--;
		return false;
	}
	_handle = hnd;
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS abierto.");
	return true;
	#else
	return false;
	#endif
}


//------------------------------------------------------------------------------------
void FSManager::close(){
    #if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		_close_internal(false);
		return;
	}
	WorkerJob job(WorkerOp::Close);
	_dispatchJob(job);
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::close()
    #endif
}


//------------------------------------------------------------------------------------
void FSManager::_close_internal(bool force){
	#if ESP_PLATFORM == 1
	if(force){
		_open_refcount = 0;
	}else if(_open_refcount > 0){
		_open_refcount--;
	}
	if(_open_refcount > 0){
		return;
	}
	if(!_handle){
		return;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Cerrando sistema NVS");
	nvs_close(_handle);
	_handle = 0;
	#endif
}


//------------------------------------------------------------------------------------
int FSManager::save(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
    #if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _save_internal(data_id, data, size, type);
	}
	WorkerJob job(WorkerOp::Save);
	job.data_id = data_id;
	job.data = data;
	job.size = size;
	job.type = type;
	_dispatchJob(job);
	return job.result_i;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::save()
    return -1;
    #endif
}


//------------------------------------------------------------------------------------
int FSManager::_save_internal(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
	#if ESP_PLATFORM == 1
	bool temp_opened = false;
	if(!_handle){
		if(!_open_internal()){
			return (int)ESP_ERR_NVS_INVALID_HANDLE;
		}
		temp_opened = true;
	}
	// Eliminamos la clave antes para obtener ese espacio
	_removeKey_internal(data_id);
	esp_err_t err = ESP_ERR_NVS_INVALID_HANDLE;
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Escribiendo %d datos en id %s...", size, data_id);
	switch(type){
		case NVSInterface::TypeUint8:
			err = nvs_set_u8(_handle, data_id, *(uint8_t*)data);
			break;
		case NVSInterface::TypeInt8:
			err = nvs_set_i8(_handle, data_id, *(int8_t*)data);
			break;
		case NVSInterface::TypeUint16:
			err = nvs_set_u16(_handle, data_id, *(uint16_t*)data);
			break;
		case NVSInterface::TypeInt16:
			err = nvs_set_i16(_handle, data_id, *(int16_t*)data);
			break;
		case NVSInterface::TypeUint32:
			err = nvs_set_u32(_handle, data_id, *(uint32_t*)data);
			break;
		case NVSInterface::TypeInt32:
			err = nvs_set_i32(_handle, data_id, *(int32_t*)data);
			break;
		case NVSInterface::TypeUint64:
			err = nvs_set_u64(_handle, data_id, *(uint64_t*)data);
			break;
		case NVSInterface::TypeInt64:
			err = nvs_set_i64(_handle, data_id, *(int64_t*)data);
			break;
		case NVSInterface::TypeString:
			err = nvs_set_str(_handle, data_id, (const char*)data);
			break;
		case NVSInterface::TypeBlob:
			err = nvs_set_blob(_handle, data_id, data, size);
			break;
		default:
			err = ESP_ERR_INVALID_ARG;
			break;
	}
	if(err != ESP_OK){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WR Error [%d] al escribir en id %s", (int)err, data_id);
		_error = (int)err;
		if(temp_opened){
			_close_internal(false);
		}
		return _error;
	}
	err = nvs_commit(_handle);
	_error = (int)err;
	if(err == ESP_OK){
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Datos escritos en id %s", data_id);
	}else{
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_COMMIT Error [%d] al escribir en id %s", (int)err, data_id);
	}
	if(temp_opened){
		_close_internal(false);
	}
	return _error;
	#else
	return -1;
	#endif
}


//------------------------------------------------------------------------------------
int FSManager::restore(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
    #if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _restore_internal(data_id, data, size, type);
	}
	WorkerJob job(WorkerOp::Restore);
	job.data_id = data_id;
	job.data = data;
	job.size = size;
	job.type = type;
	_dispatchJob(job);
	return job.result_i;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::restore()
    return -1;
    #endif
}


//------------------------------------------------------------------------------------
int FSManager::_restore_internal(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
	#if ESP_PLATFORM == 1
	bool temp_opened = false;
	if(!_handle){
		if(!_open_internal()){
			return (int)ESP_ERR_NVS_INVALID_HANDLE;
		}
		temp_opened = true;
	}
	esp_err_t err = ESP_ERR_NVS_INVALID_HANDLE;
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Leyendo %d datos de id %s...", size, data_id);
	switch(type){
		case NVSInterface::TypeUint8:
			err = nvs_get_u8(_handle, data_id, (uint8_t*)data);
			break;
		case NVSInterface::TypeInt8:
			err = nvs_get_i8(_handle, data_id, (int8_t*)data);
			break;
		case NVSInterface::TypeUint16:
			err = nvs_get_u16(_handle, data_id, (uint16_t*)data);
			break;
		case NVSInterface::TypeInt16:
			err = nvs_get_i16(_handle, data_id, (int16_t*)data);
			break;
		case NVSInterface::TypeUint32:
			err = nvs_get_u32(_handle, data_id, (uint32_t*)data);
			break;
		case NVSInterface::TypeInt32:
			err = nvs_get_i32(_handle, data_id, (int32_t*)data);
			break;
		case NVSInterface::TypeUint64:
			err = nvs_get_u64(_handle, data_id, (uint64_t*)data);
			break;
		case NVSInterface::TypeInt64:
			err = nvs_get_i64(_handle, data_id, (int64_t*)data);
			break;
		case NVSInterface::TypeString: {
			size_t required_size = (size_t)size;
			err = nvs_get_str(_handle, data_id, (char*)data, &required_size);
			break;
		}
		case NVSInterface::TypeBlob: {
			size_t blob_size = (size_t)size;
			err = nvs_get_blob(_handle, data_id, data, &blob_size);
			if(err == ESP_OK && blob_size != (size_t)size){
				DEBUG_TRACE_W(_EXPR_, _MODULE_, "WARN_SIZE. Tamaño en NVS (%d) diferente al buffer (%d) para id %s", blob_size, size, data_id);
				err = ESP_ERR_NVS_INVALID_LENGTH;
			}
			break;
		}
		default:
			err = ESP_ERR_INVALID_ARG;
			break;
	}
	_error = (int)err;
	if(err == ESP_OK){
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Datos leídos correctamente de id %s", data_id);
	}else{
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_READ. Error [%d] al leer %d datos de id %s", (int)err, size, data_id);
	}
	if(temp_opened){
		_close_internal(false);
	}
	return _error;
	#else
	return -1;
	#endif
}


//------------------------------------------------------------------------------------
bool FSManager::checkKey(const char* data_id){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _checkKey_internal(data_id);
	}
	WorkerJob job(WorkerOp::CheckKey);
	job.data_id = data_id;
	_dispatchJob(job);
	return job.result_b;
	#elif __MBED__==1
	//TODO
	#warning TODO FSManager::checkKey()
	return false;
	#endif
}


//------------------------------------------------------------------------------------
bool FSManager::_checkKey_internal(const char* data_id){
	#if ESP_PLATFORM == 1
	bool temp_opened = false;
	if(!_handle){
		if(!_open_internal()){
			return false;
		}
		temp_opened = true;
	}
	uint8_t value = 0;
	esp_err_t err = nvs_get_u8(_handle, data_id, &value);
	if(temp_opened){
		_close_internal(false);
	}
	return (err != ESP_ERR_NVS_NOT_FOUND);
	#else
	return false;
	#endif
}


//------------------------------------------------------------------------------------
int FSManager::removeKey(const char* data_id){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _removeKey_internal(data_id);
	}
	WorkerJob job(WorkerOp::RemoveKey);
	job.data_id = data_id;
	_dispatchJob(job);
	return job.result_i;
	#elif __MBED__==1
	//TODO
	#warning TODO FSManager::removeKey()
	return -1;
#endif
}


//------------------------------------------------------------------------------------
int FSManager::_removeKey_internal(const char* data_id){
	#if ESP_PLATFORM == 1
	bool temp_opened = false;
	if(!_handle){
		if(!_open_internal()){
			return (int)ESP_ERR_NVS_INVALID_HANDLE;
		}
		temp_opened = true;
	}
	esp_err_t err = nvs_erase_key(_handle, data_id);
	if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WR Error [%d] al eliminar en id %s", (int)err, data_id);
		_error = (int)err;
		if(temp_opened){
			_close_internal(false);
		}
		return _error;
	}
	err = nvs_commit(_handle);
	_error = (int)err;
	if(err == ESP_OK){
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Datos borrados en id %s", data_id);
	}else{
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_COMMIT Error [%d] al eliminar en id %s", (int)err, data_id);
	}
	if(temp_opened){
		_close_internal(false);
	}
	return _error;
	#else
	return -1;
	#endif
}

//------------------------------------------------------------------------------------
bool FSManager::erase(){
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		return _erase_internal();
	}
	WorkerJob job(WorkerOp::ErasePartition);
	_dispatchJob(job);
	return job.result_b;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::open()
    return false;
    #endif
}


//------------------------------------------------------------------------------------
bool FSManager::_erase_internal(){
	#if ESP_PLATFORM == 1
	// Cierra handle para poder borrar partición
	_close_internal(true);
	esp_err_t err = nvs_flash_erase_partition(DEFAULT_NVSInterface_Partition);
	if(err != ESP_OK){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_ERASE [%d] al borrar partición NVS", err);
		return false;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS borrado.");
	// Re-inicializa y reabre handle
	return (_init_internal() == ESP_OK);
	#else
	return false;
	#endif
}


void FSManager::list_nvs_keys() {
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		_list_nvs_keys_internal();
		return;
	}
	WorkerJob job(WorkerOp::ListKeys);
	_dispatchJob(job);
	#endif
}


//------------------------------------------------------------------------------------
void FSManager::_list_nvs_keys_internal() {
	#if ESP_PLATFORM == 1
	DEBUG_TRACE_E(true, _MODULE_, "Listando claves NVS...");
	nvs_iterator_t it = nvs_entry_find(DEFAULT_NVSInterface_Partition, NULL, NVS_TYPE_ANY);
	uint32_t keyCount = 0;
	while (it != NULL) {
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);
		DEBUG_TRACE_E(true, _MODULE_,"[%d]Key: %s", ++keyCount, info.key);
		it = nvs_entry_next(it);
	}
	nvs_release_iterator(it);
	#endif
}

void FSManager::eraseKeyList(std::vector<std::string> keys_to_manage, bool delete_only_these) {
	#if ESP_PLATFORM == 1
	if(_inWorkerContext()){
		_eraseKeyList_internal(keys_to_manage, delete_only_these);
		return;
	}
	WorkerJob job(WorkerOp::EraseKeyList);
	job.keys_to_manage = &keys_to_manage;
	job.delete_only_these = delete_only_these;
	_dispatchJob(job);
	#endif
}


//------------------------------------------------------------------------------------
void FSManager::_eraseKeyList_internal(std::vector<std::string>& keys_to_manage, bool delete_only_these) {
	#if ESP_PLATFORM == 1
	if(!_handle){
		if(!_open_internal()){
			return;
		}
	}
	nvs_iterator_t it = nvs_entry_find(DEFAULT_NVSInterface_Partition, NULL, NVS_TYPE_ANY);
	while(it != NULL){
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);
		bool key_in_list = false;
		for(uint32_t i = 0; i < keys_to_manage.size(); i++){
			if((strcmp(info.key, keys_to_manage[i].c_str()) == 0) &&
			   (strlen(info.key) == strlen(keys_to_manage[i].c_str()))){
				key_in_list = true;
				break;
			}
		}
		if((delete_only_these && key_in_list) || (!delete_only_these && !key_in_list)){
			DEBUG_TRACE_I(_EXPR_, _MODULE_, "Deleting key: %s", info.key);
			esp_err_t err = nvs_erase_key(_handle, info.key);
			if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND){
				DEBUG_TRACE_E(_EXPR_, _MODULE_, "Error deleting key: %s", esp_err_to_name(err));
			}
		}
		it = nvs_entry_next(it);
	}
	nvs_release_iterator(it);
	nvs_commit(_handle);
	#endif
}
