/*
 * FSManager.cpp
 *
 *  Created on: Sep 2017
 *      Author: raulMrello
 */

#include "FSManager.h"


//------------------------------------------------------------------------------------
//--- STATIC TYPES ------------------------------------------------------------------
//------------------------------------------------------------------------------------


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

#if ESP_PLATFORM == 1


//------------------------------------------------------------------------------------
FSManager::FSManager(const char *name, PinName mosi, PinName miso, PinName sclk, PinName csel, int freq, bool defdbg) : NVSInterface(name) {
	_ready = false;
	_defdbg = defdbg;
	// inicializo
	_mtx.lock();
	init();
	_mtx.unlock();
}


//------------------------------------------------------------------------------------
int FSManager::init() {
	// Initialize NVS
	esp_err_t err = nvs_flash_init_partition(DEFAULT_NVSInterface_Partition);
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
}


//------------------------------------------------------------------------------------
bool FSManager::open(){
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
}


//------------------------------------------------------------------------------------
void FSManager::close(){
	if(!_handle){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_HND, Handle nulo en <close>");
		_mtx.unlock();
		return;
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Cerrando sistema NVS");
	nvs_close(_handle);
	_handle = 0;
	_mtx.unlock();
}


//------------------------------------------------------------------------------------
int FSManager::save(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
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
}


//------------------------------------------------------------------------------------
int FSManager::restore(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type){
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
}

#endif

#if __MBED__ == 1
//------------------------------------------------------------------------------------
FSManager::FSManager(const char *name, PinName mosi, PinName miso, PinName sclk, PinName csel, int freq) : FATFileSystem(name), NVSInterface(name) {

    // Creo el block device para la spi flash
    _bd = new SPIFBlockDevice(mosi, miso, sclk, csel, freq);
    _bd->init();
    
    // Monto el sistema de ficheros en el blockdevice
    _error = FATFileSystem::mount(_bd);
    
    // Chequeo si hay información de formato, en caso de error formateo y creo archivo
    if(!ready()){
        _error = FATFileSystem::format(_bd);
        FILE* fd = fopen("/fs/format_info.txt", "w");
        if(fd){
            _error = fputs("Formateado correctamente.", fd);
            _error = fclose(fd);
        }
    }
}


//------------------------------------------------------------------------------------
bool FSManager::ready() {
    FILE* fd = fopen("/fs/format_info.txt", "r");
    if(!fd){
        return false;
    }
    char buff[32] = {0};
    if(fgets(&buff[0], 32, fd) == 0){
        _error = fclose(fd);
        return false;
    }
    _error = fclose(fd);
    if(strcmp(buff, "Formateado correctamente.) != 0){
        return false;
    }
    return true;
}


//------------------------------------------------------------------------------------
int FSManager::save(const char* data_id, void* data, uint32_t size){
    char * filename = (char*)Heap::memAlloc(strlen(data_id) + strlen("/fs/.dat") + 1);
    if(!filename){
        return -1;
    }
    sprintf(filename, "/fs/%s.dat", data_id);
    FILE* fd = fopen(filename, "w");
    int written = 0;
    if(fd){
        // reescribe desde el comienzo
        fseek(fd, 0, SEEK_SET);
        if(data && size){
            written = fwrite(data, 1, size, fd);
        }
        fclose(fd);
    }
    Heap::memFree(filename);
    return written;
}


//------------------------------------------------------------------------------------
int FSManager::restore(const char* data_id, void* data, uint32_t size){
    char * filename = (char*)Heap::memAlloc(strlen(data_id) + strlen("/fs/.dat") + 1);
    if(!filename){
        return -1;
    }
    sprintf(filename, "/fs/%s.dat", data_id);
    FILE* fd = fopen(filename, "r");
    int rd = 0;
    if(fd){
        if(data && size){
            rd = fread(data, 1, size, fd);
        }
        fclose(fd);
    }
    Heap::memFree(filename);
    return rd;
}


//------------------------------------------------------------------------------------
int32_t FSManager::openRecordSet(const char* data_id){
    char * filename = (char*)Heap::memAlloc(strlen(data_id) + strlen("/fs/.dat") + 1);
    if(!filename){
        return 0;
    }
    sprintf(filename, "/fs/%s.dat", data_id);
    FILE* fd = fopen(filename, "r+");
    Heap::memFree(filename);
    return (int32_t)fd;    
}


//------------------------------------------------------------------------------------
int32_t FSManager::closeRecordSet(int32_t recordset){
    return (int32_t)fclose((FILE*)recordset);
}


//------------------------------------------------------------------------------------
int32_t FSManager::writeRecordSet(int32_t recordset, void* data, uint32_t record_size, int32_t* pos){
    if(!recordset || !data || !record_size){
        return 0;
    }
    int wr = 0;
    int32_t vpos = (pos)? (*pos) : 0;
    // se sitúa en la posición deseada
    if(pos){
        fseek((FILE*)recordset, vpos, SEEK_SET);
    }
    // actualiza el registro
    wr = fwrite(data, 1, record_size, (FILE*)recordset);    
    vpos += wr;
    if(pos){
        *pos = vpos;
    }
    return wr;
}

//------------------------------------------------------------------------------------
int32_t FSManager::readRecordSet(int32_t recordset, void* data, uint32_t record_size, int32_t* pos){
    if(!recordset || !data || !record_size){
        return 0;
    }
    int rd = 0;
    int32_t vpos = (pos)? (*pos) : 0;
    // se sitúa en la posición deseada, o a continuación
    if(pos){
        fseek((FILE*)recordset, vpos, SEEK_SET);
    }
    // lee el registro
    rd = fread(data, 1, record_size, (FILE*)recordset);
    vpos += rd;
    if(pos){
        *pos = vpos;
    }
    return rd;
}


//------------------------------------------------------------------------------------
int32_t FSManager::getRecord(const char* data_id, void* data, uint32_t record_size, int32_t* pos){
    if(!data || !record_size){
        return 0;
    }
    char * filename = (char*)Heap::memAlloc(strlen(data_id) + strlen("/fs/.dat") + 1);
    if(!filename){
        return -1;
    }
    sprintf(filename, "/fs/%s.dat", data_id);
    FILE* fd = fopen(filename, "r");
    int rd = 0;
    int32_t vpos = (pos)? (*pos) : 0;
    if(fd){
        // se sitúa en la posición deseada
        fseek(fd, vpos, SEEK_SET);
        // lee el registro
        rd = fread(data, 1, record_size, fd);
        fclose(fd);
    }
    vpos += rd;
    if(pos){
        *pos = vpos;
    }
    return rd;
}


//------------------------------------------------------------------------------------
int32_t FSManager::setRecord(const char* data_id, void* data, uint32_t record_size, int32_t* pos){
    if(!data || !record_size){
        return 0;
    }
    char * filename = (char*)Heap::memAlloc(strlen(data_id) + strlen("/fs/.dat") + 1);
    if(!filename){
        return -1;
    }
    sprintf(filename, "/fs/%s.dat", data_id);
    FILE* fd = fopen(filename, "r+");
    int wr = 0;
    int32_t vpos = (pos)? (*pos) : 0;
    if(fd){
        // se sitúa en la posición deseada
        fseek(fd, vpos, SEEK_SET);
        // actualiza el registro
        wr = fwrite(data, 1, record_size, fd);
        fclose(fd);
    }
    vpos += wr;
    if(pos){
        *pos = vpos;
    }
    return wr;
}
#endif


