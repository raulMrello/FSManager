/*
 * FSManager.cpp
 *
 *  Created on: Sep 2017
 *      Author: raulMrello
 */

#include "FSManager.h"
#if ESP_PLATFORM == 1
#include "nvs.h"
#endif

//------------------------------------------------------------------------------------
//--- STATIC TYPES ------------------------------------------------------------------
//------------------------------------------------------------------------------------

FSManager* FSManager::_static_instance = NULL;

//------------------------------------------------------------------------------------
//--- PRIVATE TYPES ------------------------------------------------------------------
//------------------------------------------------------------------------------------

/** Macro para imprimir trazas de depuración, siempre que se haya configurado un objeto
 *	Logger válido (ej: _debug)
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
	// inicializo
	_mtx.lock();
	init();
	_mtx.unlock();
    #elif __MBED__ == 1
    //TODO
    #warning TODO FSManager::FSManager()
    #endif
	_static_instance = this;
}


//------------------------------------------------------------------------------------
int FSManager::init() {
    #if ESP_PLATFORM == 1
	// Initialize NVS and the default partition
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// NVS partition was truncated and needs to be erased
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );
	if(err != ESP_OK){
		return ESP_FAIL;
	}

	err = nvs_flash_init_partition(DEFAULT_NVSInterface_Partition);
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// NVS partition was truncated and needs to be erased
		ESP_ERROR_CHECK(nvs_flash_erase_partition(DEFAULT_NVSInterface_Partition));
		err = nvs_flash_init_partition(DEFAULT_NVSInterface_Partition);
	}
	ESP_ERROR_CHECK( err );
	if(err != ESP_OK){
		return ESP_FAIL;
	}

	// Open
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Chequeando sistema NVS ");

	err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &_handle);
	if (err != ESP_OK) {
		_handle = (nvs_handle)NULL;
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN [%d]. No se puede abrir el sistema NVS", err);
		return err;
	}
	nvs_close(_handle);
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS OK!");
	_ready = true;
	return err;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::init()
    return -1;
    #endif
}


//------------------------------------------------------------------------------------
bool FSManager::open(){
    #if ESP_PLATFORM == 1
	_mtx.lock();
	nvs_handle hnd;
	esp_err_t err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &hnd);
	if (err != ESP_OK) {
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN [%d] al abrir el sistema NVS", err);
		_mtx.unlock();
		return false;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS abierto.");
	_handle = hnd;
	return true;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::open()
    return false;
    #endif
}


//------------------------------------------------------------------------------------
void FSManager::close(){
    #if ESP_PLATFORM == 1
	if(!_handle){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_HND, Handle nulo en <close>");
		_mtx.unlock();
		return;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Cerrando sistema NVS");
	nvs_close(_handle);
	_handle = 0;
	_mtx.unlock();
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::close()
    #endif
}


//------------------------------------------------------------------------------------
int FSManager::save(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
    #if ESP_PLATFORM == 1
	esp_err_t err = ESP_ERR_NVS_INVALID_HANDLE;
	if(!_handle){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_HND, Handle nulo en <save>");
		return (int)err;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Escribiendo %d datos en id %s...", size, data_id);
    switch(type){
    	case NVSInterface::TypeUint8:{
    		err = nvs_set_u8(_handle, data_id, *(uint8_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt8:{
    		err = nvs_set_i8(_handle, data_id, *(int8_t*)data);
    		break;
    	}
    	case NVSInterface::TypeUint16:{
    		err = nvs_set_u16(_handle, data_id, *(uint16_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt16:{
    		err = nvs_set_i16(_handle, data_id, *(int16_t*)data);
    		break;
    	}
    	case NVSInterface::TypeUint32:{
    		err = nvs_set_u32(_handle, data_id, *(uint32_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt32:{
    		err = nvs_set_i32(_handle, data_id, *(int32_t*)data);
    		break;
    	}
    	case NVSInterface::TypeUint64:{
    		err = nvs_set_u64(_handle, data_id, *(uint64_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt64:{
    		err = nvs_set_i64(_handle, data_id, *(int64_t*)data);
    		break;
    	}
    	case NVSInterface::TypeString:{
    		err = nvs_set_str (_handle, data_id, (const char*)data);
    		break;
    	}
    	case NVSInterface::TypeBlob:{
    		err = nvs_set_blob(_handle, data_id, data, size);
    		break;
    	}
    	default:{
    		err = ESP_ERR_INVALID_ARG;
    		break;
    	}
    }
    if(err != ESP_OK){
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WR Error [%d] al escribir en id %s", (int)err, data_id);
    	_error = (int)err;
    	return _error;
    }
    err = nvs_commit(_handle);
    if(err == ESP_OK){
    	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Datos escritos en id %s", data_id);
    	_error = (int)err;
    	return _error;
    }
    DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_COMMIT Error [%d] al escribir en id %s", (int)err, data_id);
    _error = (int)err;
    return _error;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::save()
    return -1;
    #endif
}


//------------------------------------------------------------------------------------
int FSManager::restore(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
    #if ESP_PLATFORM == 1
	esp_err_t err = ESP_ERR_NVS_INVALID_HANDLE;
	if(!_handle){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_HND, Handle nulo en <restore>");
		return (int)err;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Leyendo %d datos de id %s...", size, data_id);
	switch(type){
    	case NVSInterface::TypeUint8:{
    		err = nvs_get_u8(_handle, data_id, (uint8_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt8:{
    		err = nvs_get_i8(_handle, data_id, (int8_t*)data);
    		break;
    	}
    	case NVSInterface::TypeUint16:{
    		err = nvs_get_u16(_handle, data_id, (uint16_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt16:{
    		err = nvs_get_i16(_handle, data_id, (int16_t*)data);
    		break;
    	}
    	case NVSInterface::TypeUint32:{
    		err = nvs_get_u32(_handle, data_id, (uint32_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt32:{
    		err = nvs_get_i32(_handle, data_id, (int32_t*)data);
    		break;
    	}
    	case NVSInterface::TypeUint64:{
    		err = nvs_get_u64(_handle, data_id, (uint64_t*)data);
    		break;
    	}
    	case NVSInterface::TypeInt64:{
    		err = nvs_get_i64(_handle, data_id, (int64_t*)data);
    		break;
    	}
    	case NVSInterface::TypeString:{
    		err = nvs_get_str (_handle, data_id, (char*)data, (size_t*)&size);
    		break;
    	}
    	case NVSInterface::TypeBlob:{
    		err = nvs_get_blob(_handle, data_id, data, (size_t*)&size);
    		break;
    	}
    	default:{
    		err = ESP_ERR_INVALID_ARG;
    		break;
    	}
    }
	_error = (int)err;
    if(err == ESP_OK){
    	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Datos leídos correctamente de id %s", data_id);
    	return _error;
    }
    DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_READ. Error [%d] al leer %d datos de id %s", (int)err, size, data_id);
    return _error;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::restore()
    return -1;
    #endif
}


//------------------------------------------------------------------------------------
bool FSManager::checkKey(const char* data_id){
	#if ESP_PLATFORM == 1
	uint8_t data=0;
	auto err = nvs_get_u8(_handle, data_id, (uint8_t*)data);
	if(err == ESP_ERR_NVS_NOT_FOUND)
		return false;
	return true;
	#elif __MBED__==1
	//TODO
	#warning TODO FSManager::checkKey()
	return false;
	#endif
}


//------------------------------------------------------------------------------------
int FSManager::removeKey(const char* data_id){
	#if ESP_PLATFORM == 1
	return (int)nvs_erase_key(_handle, data_id);
	#elif __MBED__==1
	//TODO
	#warning TODO FSManager::removeKey()
	return -1;
#endif
}

//------------------------------------------------------------------------------------
bool FSManager::erase(){
	#if ESP_PLATFORM == 1
	_mtx.lock();
	nvs_handle hnd;
	esp_err_t err = nvs_flash_erase_partition(DEFAULT_NVSInterface_Partition);
	if (err != ESP_OK) {
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_ERASE [%d] al abrir el sistema NVS", err);
		_mtx.unlock();
		return false;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS borrado.");
	return true;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::open()
    return false;
    #endif
}

