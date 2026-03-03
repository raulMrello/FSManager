/*
 * FSManager.h
 *
 *  Created on: Sep 2017
 *      Author: raulMrello
 *
 *	FSManager es el m�dulo encargado de gestionar el acceso al sistema de ficheros. Es una implementaci�n de la clase
 *  FATFileSystem, heredando por lo tanto sus miembros p�blicos.
 *  El soporte del sistema de ficheros corre sobre una memoria NOR-Flash SPI SST6VFX de Microchip y por lo tanto se
 *  implementa un SPIFBlockDevice.
 *
 *  Este m�dulo se ejecuta como una librer�a pasiva, es decir, corriendo en el contexto del objeto llamante, y por lo
 *  tanto carece de thread asociado
 */
 
#ifndef __FSManager__H
#define __FSManager__H

#include "mbed.h"
#include "Heap.h"
#include "NVSInterface.h"
#include <vector>
#include <string>
#if ESP_PLATFORM == 1
#include "FATInterface.h"
#endif
#if __MBED__==1
#include "mdf_api_cortex.h"
#endif



#define FSManager_DEBUG		1


class FSManager : public NVSInterface{

  public:

    /** Constructor
     *  Crea el gestor del sistema de ficheros, que puede ser un sistema FAT o un sistema KEY-VALUE. Al cual se le
     *  asocia un nombre y en el caso de utilizar una SPI_FLASH externa, los gpio del puerto spi utilizado
     *  @param name Nombre del sistema de ficheros
     *  @param mosi Salida de datos SPI. Por defecto no utilizado (NC)
     *  @param miso Entrada de datos SPI. Por defecto no utilizado (NC)
     *  @param sclk Reloj SPI en modo master. Por defecto no utilizado (NC)
     *  @param csel Salida NSS en modo master gestionada por hardware. Por defecto no utilizado (NC)
     *  @param freq Frecuencia SPI (40MHz o 20MHz dependiendo del puerto utilizado).. Por defecto no utilizado (0)
     *  @param defdbg Flag para activar o desactivar el canal de depuraci�n por defecto
     */
    FSManager(const char *name, PinName32 mosi=NC, PinName32 miso=NC, PinName32 sclk=NC, PinName32 csel=NC, int freq=0, bool defdbg = false);
    virtual ~FSManager();
  
    /** init
     *  Inicializa el sistema de ficheros
     *  @return 0 (correcto), <0 (c�digo de error)
     */
    virtual int init();


	/** Habilita canal de depuraci�n por defecto <printf>
     *  @param endis Flag para activar o desactivar el canal de depuraci�n por defecto
     */
    void setDebugChannel(bool defdbg) {_defdbg = defdbg; }
  
  
    /** ready
     *  Chequea si el sistema de ficheros est� listo
     *  @return True (si tiene formato) o False (si tiene errores)
     */
    virtual bool ready() {return _ready; }


    /** Abre el handle para realizar varias operaciones en bloque
     *
     * @return True: Handle abierto, False: Handle no abierto (error)
     */
    virtual bool open();


    /** cierra el handle
     *
     */
    virtual void close();
  

    /** save
     *  Graba datos en memoria no vol�til de acuerdo a un identificador dado
     *  @param data_id Identificador de los datos a grabar
     *  @param data  Puntero a los datos 
     *  @param size Tama�o de los datos en bytes
     *  @param type tipo de dato
     *  @return Resultado de la operaci�n (error=-1, num_datos_escritos >= 0)
     */   
    virtual int save(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type);
  
  
    /** restore
     *  Recupera datos de memoria no vol�til de acuerdo a un identificador dado
     *  @param data_id Identificador de los datos a grabar
     *  @param data  Puntero que recibe los datos recuperados
     *  @param size Tama�o m�ximo de datos a recuperar
     *  @param type tipo de dato
     *  @return Resultado de la operaci�n (error=-1, num_datos recuperados >= 0)
     */   
    virtual int restore(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type);


    /** checkKey
     *  Chequea si una clave existe
     *  @param data_id Identificador de la clave
     *  @return true: existe
     */
    virtual bool checkKey(const char* data_id);


    /** removeKey
     *  Elimina una clave
     *  @param data_id Identificador de la clave
     *  @return c�digo de error
     */
    virtual int removeKey(const char* data_id);
    /**
     * Devuelve la instancia est�tica
     * @return
     */
    static FSManager* getStaticInstance(){ return _static_instance; }

    /*
     * Borra la partici�n NVS creada
     * */
    virtual bool erase();

    void list_nvs_keys();

    void eraseKeyList(std::vector<std::string> keys_to_manage, bool delete_only_these);

protected:

	/** Flag para habilitar trazas de depuraci�n por defecto */
	bool _defdbg;

	/** Mutex de acceso al sistema NVS */
	Mutex _mtx;

private:

	/** Propiedades heredadas de NVSInterface */
	// const char* _name;          /// Nombre del sistema de ficheros
	// int _error;                 /// �ltimo error registrado

	#if ESP_PLATFORM == 1
	nvs_handle _handle;
	#endif

  #if ESP_PLATFORM == 1
  // ---------------------------------------------------------------------------------
  // Worker interno: todas las llamadas a NVS se ejecutan en este hilo
  // (stack en RAM interna) para que los hilos de ActiveModule puedan ir
  // opcionalmente a memoria externa sin arrastrar las restricciones de NVS.
  // ---------------------------------------------------------------------------------
  enum class WorkerOp : uint8_t {
    Init,
    Open,
    Close,
    Save,
    Restore,
    CheckKey,
    RemoveKey,
    ErasePartition,
    ListKeys,
    EraseKeyList,
    Stop
  };

  struct WorkerJob {
    WorkerOp op;
    const char* data_id;
    void* data;
    uint32_t size;
    NVSInterface::KeyValueType type;
    std::vector<std::string>* keys_to_manage;
    bool delete_only_these;
    int result_i;
    bool result_b;
    Semaphore done;

    explicit WorkerJob(WorkerOp op_) :
      op(op_),
      data_id(NULL),
      data(NULL),
      size(0),
      type(NVSInterface::TypeUint8),
      keys_to_manage(NULL),
      delete_only_these(false),
      result_i(0),
      result_b(false),
      done(0, 1) {
    }
  };

  static constexpr uint32_t WorkerQueueDepth = 16;
  Thread* _worker_th = NULL;
  Semaphore _worker_started{0, 1};
  Queue<WorkerJob, WorkerQueueDepth> _worker_queue;
  osThreadId _worker_tid = NULL;
  bool _worker_ok = false;
  int _open_refcount = 0;

  void _ensureWorker();
  bool _inWorkerContext() const;
  void _workerTask();
  void _dispatchJob(WorkerJob& job);

  int _init_internal();
  bool _open_internal();
  void _close_internal(bool force);
  int _save_internal(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type);
  int _restore_internal(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type);
  bool _checkKey_internal(const char* data_id);
  int _removeKey_internal(const char* data_id);
  bool _erase_internal();
  void _list_nvs_keys_internal();
  void _eraseKeyList_internal(std::vector<std::string>& keys_to_manage, bool delete_only_these);
  #endif

	/** Flag para indicar el estado del componente */
	bool _ready;

	/** instancia est�tica */
	static FSManager* _static_instance;

};
     
#endif /*__FSManager__H */

/**** END OF FILE ****/


