/*
 * FSManager.h
 *
 *  Created on: Sep 2017
 *      Author: raulMrello
 *
 *	FSManager es el módulo encargado de gestionar el acceso al sistema de ficheros. Es una implementación de la clase
 *  FATFileSystem, heredando por lo tanto sus miembros públicos.
 *  El soporte del sistema de ficheros corre sobre una memoria NOR-Flash SPI SST6VFX de Microchip y por lo tanto se
 *  implementa un SPIFBlockDevice.
 *
 *  Este módulo se ejecuta como una librería pasiva, es decir, corriendo en el contexto del objeto llamante, y por lo
 *  tanto carece de thread asociado
 */
 
#ifndef __FSManager__H
#define __FSManager__H

#include "mbed.h"
#include "Heap.h"
#include "NVSInterface.h"
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
     *  @param defdbg Flag para activar o desactivar el canal de depuración por defecto
     */
    FSManager(const char *name, PinName32 mosi=NC, PinName32 miso=NC, PinName32 sclk=NC, PinName32 csel=NC, int freq=0, bool defdbg = false);
    virtual ~FSManager(){
    	_static_instance = NULL;
    }
  
    /** init
     *  Inicializa el sistema de ficheros
     *  @return 0 (correcto), <0 (código de error)
     */
    virtual int init();


	/** Habilita canal de depuración por defecto <printf>
     *  @param endis Flag para activar o desactivar el canal de depuración por defecto
     */
    void setDebugChannel(bool defdbg) {_defdbg = defdbg; }
  
  
    /** ready
     *  Chequea si el sistema de ficheros está listo
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
     *  Graba datos en memoria no volátil de acuerdo a un identificador dado
     *  @param data_id Identificador de los datos a grabar
     *  @param data  Puntero a los datos 
     *  @param size Tamaño de los datos en bytes
     *  @param type tipo de dato
     *  @return Resultado de la operación (error=-1, num_datos_escritos >= 0)
     */   
    virtual int save(const char* data_id, void* data, uint32_t size, NVSInterface::KeyValueType type);
  
  
    /** restore
     *  Recupera datos de memoria no volátil de acuerdo a un identificador dado
     *  @param data_id Identificador de los datos a grabar
     *  @param data  Puntero que recibe los datos recuperados
     *  @param size Tamaño máximo de datos a recuperar
     *  @param type tipo de dato
     *  @return Resultado de la operación (error=-1, num_datos recuperados >= 0)
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
     *  @return código de error
     */
    virtual int removeKey(const char* data_id);
    /**
     * Devuelve la instancia estática
     * @return
     */
    static FSManager* getStaticInstance(){ return _static_instance; }

    /*
     * Borra la partición NVS creada
     * */
    virtual bool erase();

protected:

	/** Flag para habilitar trazas de depuración por defecto */
	bool _defdbg;

	/** Mutex de acceso al sistema NVS */
	Mutex _mtx;

private:

	/** Propiedades heredadas de NVSInterface */
	// const char* _name;          /// Nombre del sistema de ficheros
	// int _error;                 /// Último error registrado

	#if ESP_PLATFORM == 1
	nvs_handle _handle;
	#endif

	/** Flag para indicar el estado del componente */
	bool _ready;

	/** instancia estática */
	static FSManager* _static_instance;

};
     
#endif /*__FSManager__H */

/**** END OF FILE ****/


