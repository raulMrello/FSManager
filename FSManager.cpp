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
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        // Retry initialization
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_flash_init_partition(DEFAULT_NVSInterface_Partition);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) 
    {
        // NVS partition was truncated and needs to be erased
        if (nvs_flash_erase_partition(DEFAULT_NVSInterface_Partition) != ESP_OK)
            return ESP_FAIL;
        
        err = nvs_flash_init_partition(DEFAULT_NVSInterface_Partition);
    }
    
    if (err != ESP_OK) 
    {
        return ESP_FAIL;
    }

    // Open
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Chequeando sistema NVS, %s name:%s", DEFAULT_NVSInterface_Partition, _name);

    err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &_handle);

    if (err != ESP_OK) 
    {
        //_handle = static_cast < nvs_handle_t > (0);
        DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN [%s]. No se puede abrir el sistema NVS", esp_err_to_name(err));
        return err;
    }
    nvs_close(_handle);
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS OK!");
    _ready = true;
    return err;
#elif __MBED__==1
    return -1;
#endif
}


//------------------------------------------------------------------------------------
bool FSManager::open(){
    #if ESP_PLATFORM == 1
	_mtx.lock();
	nvs_handle_t hnd;
	esp_err_t err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &hnd);
	if (err != ESP_OK) {
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN [%d] al abrir el sistema NVS", err);
		_mtx.unlock();
		return false;
	}
	DEBUG_TRACE_E(_EXPR_, _MODULE_, "Sistema NVS abierto con handle=%p", (void*)hnd);
	_handle = hnd;
	_mtx.unlock();
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
	_mtx.lock();
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
int FSManager::save(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type) {
#if ESP_PLATFORM == 1
    if (!_ready) {
    DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_STATE: NVS no inicializado");
        return (int)ESP_ERR_INVALID_STATE;
    }
    if (data_id == nullptr || data_id[0] == '\0') {
    DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_ARG: key invalida");
        return (int)ESP_ERR_INVALID_ARG;
    }
    
    // Zona crítica
    _mtx.lock();
    esp_err_t err = ESP_OK;

    // Abre handle si está cerrado
    if (_handle == 0) {
        err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &_handle);
        if (err != ESP_OK) {
            DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN (%s/%s): %s",
                          DEFAULT_NVSInterface_Partition, _name, esp_err_to_name(err));
            _mtx.unlock();
            return (int)err;
        }
    }
    
    // Escritura por tipo (validaciones extra en string/blob)
    switch (type) {
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
        case NVSInterface::TypeString: {
            const char* s = (const char*)data;
            if (s == nullptr) { err = ESP_ERR_INVALID_ARG; break; }
            // Debe ser null-terminated; si viene de un buffer no garantizado, duplica antes.
            err = nvs_set_str(_handle, data_id, s);
            break;
        }
        case NVSInterface::TypeBlob: {
            // En IDF 5.x, si size==0 asegúrate de pasar data==NULL o size==0 consistentemente
            const void* p = (size > 0) ? data : nullptr;
            err = nvs_set_blob(_handle, data_id, p, (size_t)size);
            break;
        }
        default:
            err = ESP_ERR_INVALID_ARG;
            break;
    }

    if (err != ESP_OK) {
        DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WR [%s] al escribir key '%s': %s",
                      DEFAULT_NVSInterface_Partition, data_id, esp_err_to_name(err));
        _mtx.unlock();
        _error = (int)err;
        return _error;
    }
    // Commit (serializado por el mismo mutex)
    err = nvs_commit(_handle);
    if (err != ESP_OK) {
        DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_COMMIT en key '%s': %s",
                      data_id, esp_err_to_name(err));
        _mtx.unlock();
        _error = (int)err;
        return _error;
    }

    DEBUG_TRACE_D(_EXPR_, _MODULE_, "OK: escrito '%s' (%u bytes) y commit hecho",
                  data_id, (unsigned)size);
    _mtx.unlock();
    _error = (int)ESP_OK;
    return _error;
#elif __MBED__==1
    return -1;
#endif
}


//------------------------------------------------------------------------------------
int FSManager::restore(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
    #if ESP_PLATFORM == 1
    esp_err_t err = ESP_OK;
    if(_handle == 0){
        // Abrimos automáticamente igual que save()
        err = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &_handle);
        if (err != ESP_OK) {
            DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_OPEN en restore (%s/%s): %s", DEFAULT_NVSInterface_Partition, _name, esp_err_to_name(err));
            return (int)err;
        }
    }
	// size es uint32_t, usar %lu
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Leyendo %lu datos de id %s...", (unsigned long)size, data_id);
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
    		size_t required_size = (size_t)size;
            err = nvs_get_str (_handle, data_id, (char*)data, &required_size);
    		break;
    	}
    	case NVSInterface::TypeBlob:{
    		size_t blob_size = 0;
            err = nvs_get_blob(_handle, data_id, NULL, &blob_size);
            if(err != ESP_OK){
                break;
            }
            if(blob_size != (size_t)size){
                DEBUG_TRACE_W(_EXPR_, _MODULE_, "WARN_SIZE. Tamaño en NVS (%d) diferente al buffer (%d) para id %s", blob_size, size, data_id);
                err = ESP_ERR_NVS_INVALID_LENGTH;
                break;
            }
            err = nvs_get_blob(_handle, data_id, data, &blob_size);
    		break;
    	}
    	default:{
    		err = ESP_ERR_INVALID_ARG;
    		break;
    	}
    }
	_error = (int)err;
    if(err == ESP_OK){
        DEBUG_TRACE_D(_EXPR_, _MODULE_, "Datos leidos correctamente de id %s", data_id);
    	return _error;
    }
	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_READ. Error [%d] al leer %lu datos de id %s", (int)err, (unsigned long)size, data_id);
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
    if(_handle == 0){
        // Intento abrir para comprobar
        nvs_handle_t tmp;
        esp_err_t e = nvs_open_from_partition(DEFAULT_NVSInterface_Partition, _name, NVS_READWRITE, &tmp);
        if(e != ESP_OK){
            return false;
        }
        nvs_close(tmp); // sólo comprobación
    }
    uint8_t data=0;
    esp_err_t err = nvs_get_u8(_handle, data_id, &data);
    return (err == ESP_OK);
	#elif __MBED__==1
	//TODO
	#warning TODO FSManager::checkKey()
	return false;
	#endif
}


//------------------------------------------------------------------------------------
int FSManager::removeKey(const char* data_id){
	#if ESP_PLATFORM == 1
	esp_err_t err = ESP_ERR_NVS_INVALID_HANDLE;
	if(!_handle){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_HND, Handle nulo en <save>");
		return (int)err;
	}
	err = nvs_erase_key(_handle, data_id);
	if(err != ESP_OK){
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WR Error [%d] al eliminar en id %s", (int)err, data_id);
    	_error = (int)err;
    	return _error;
    }

	err = nvs_commit(_handle);
	if(err == ESP_OK){
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Datos borrados en id %s", data_id);
		_error = (int)err;
		return _error;
	}

	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_COMMIT Error [%d] al eliminar en id %s", (int)err, data_id);
    _error = (int)err;
    return _error;
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
	esp_err_t err = nvs_flash_erase_partition(DEFAULT_NVSInterface_Partition);
	if (err != ESP_OK) {
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_ERASE [%d] al abrir el sistema NVS", err);
		_mtx.unlock();
		return false;
	}
	//if (nvs_flash_deinit_partition(DEFAULT_NVSInterface_Partition) != ESP_OK)
	//	return false;
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sistema NVS borrado.");
	_mtx.unlock();
	return true;
    #elif __MBED__==1
    //TODO
    #warning TODO FSManager::open()
    return false;
    #endif
}


void FSManager::list_nvs_keys() {
	#if ESP_PLATFORM == 1
    //nvs_flash_init();  // Inicializa NVS
	DEBUG_TRACE_E(true, _MODULE_, "Listando claves NVS...");

    nvs_iterator_t it = nullptr;
    esp_err_t res = nvs_entry_find(DEFAULT_NVSInterface_Partition, nullptr, NVS_TYPE_ANY, &it);
    if(res != ESP_OK){
        DEBUG_TRACE_W(true, _MODULE_, "No se pudo iniciar iteracion NVS (err=%s)", esp_err_to_name(res));
        return;
    }
    uint32_t keyCount = 0;
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        DEBUG_TRACE_E(true, _MODULE_,"[%lu]Key: %s", ++keyCount, info.key);
        esp_err_t nxt = nvs_entry_next(&it);
        if(nxt != ESP_OK){
            if(nxt != ESP_ERR_NVS_NOT_FOUND){
                DEBUG_TRACE_W(true, _MODULE_, "Fin iteracion err=%s", esp_err_to_name(nxt));
            }
            break;
        }
    }

	// Libera iterator (aunque it ya será NULL si terminó correctamente)
    nvs_release_iterator(it);
	#endif
}

void FSManager::eraseKeyList(std::vector<std::string> keys_to_manage, bool delete_only_these) {
	#if ESP_PLATFORM == 1
	esp_err_t err = ESP_OK;
	open();
    // Iterador para recorrer todas las claves
	nvs_iterator_t it = nullptr;
    esp_err_t res  = nvs_entry_find(DEFAULT_NVSInterface_Partition, nullptr, NVS_TYPE_ANY, &it);
    if(res != ESP_OK){
        DEBUG_TRACE_W(_EXPR_, _MODULE_, "No se pudo iniciar iteracion NVS (err=%s)", esp_err_to_name(res));
        close();
        return;
    }
    while (it != nullptr) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        // Verifica si la clave está en la lista
		// recorremos el vector de claves a borrar
		bool key_in_list = false;
		for(uint32_t i=0; i<keys_to_manage.size(); i++){
			if((strcmp(info.key, keys_to_manage[i].c_str()) == 0) &&
			   (strlen(info.key) == strlen(keys_to_manage[i].c_str()))){
				key_in_list = true;
				break;
			}
		}

        if ((delete_only_these && key_in_list) || (!delete_only_these && !key_in_list)) {
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Deleting key: %s", info.key);
            err = nvs_erase_key(_handle, info.key);
            if (err != ESP_OK) {
                DEBUG_TRACE_E(_EXPR_, _MODULE_, "Error deleting key: %s", esp_err_to_name(err));
            }
        }

        esp_err_t nxt = nvs_entry_next(&it);
        if(nxt != ESP_OK){
            if(nxt != ESP_ERR_NVS_NOT_FOUND){
                DEBUG_TRACE_W(_EXPR_, _MODULE_, "Fin iteracion err=%s", esp_err_to_name(nxt));
            }
            break;
        }
    }

    // Libera el iterador
    nvs_release_iterator(it);

    // Guarda los cambios
    nvs_commit(_handle);
	close();
	#endif
}
