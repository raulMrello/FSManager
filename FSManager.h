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

#if __MBED__ == 1
#include "SPIFBlockDevice.h"
#include "FATFileSystem.h"
#endif


#define FSManager_DEBUG		1


#if __MBED__ == 1
class FSManager : public FATFileSystem, public NVSInterface{
#else
class FSManager : public NVSInterface{
#endif

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
    FSManager(const char *name, PinName mosi=NC, PinName miso=NC, PinName sclk=NC, PinName csel=NC, int freq=0, bool defdbg = false);
    virtual ~FSManager(){
    }
  
    /** init
     *  Inicializa el sistema de ficheros
     *  @return 0 (correcto), <0 (c�digo de error)
     */
    #if __MBED__ == 1
    virtual int init(){
        return 0;
    }  
    #else
    virtual int init();
    #endif


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



	#if __MBED__ == 1

	 /** getBlockDevice
	 *  Obtiene una referencia al BlockDevice implementado
	 *  @return _name Nombre asignado
	 */
	BlockDevice* getBlockDevice() { return _bd; }



	/** openRecordSet
	 *  Abre un manejador de registros a partir de un identificador
	 *  @param data_id Identificador del recordset a abrir
	 *  @return identificador del recordset o 0 en caso de error
	 */
	int32_t openRecordSet(const char* data_id);

	/** closeRecordSet
	 *  Cierra un manejador de registros a partir de un identificador
	 *  @param recordset Manejador de registros a cerrar
	 *  @return resultado de la operaci�n: 0 (ok), !=0 (error)
	 */
	int32_t closeRecordSet(int32_t recordset);


	/** writeRecordSet
	 *  Inserta un registro de un tama�o desde una posici�n dada, en un recordset abierto previamente
	 *  @param recordset Identificador del manejador de registros
	 *  @param data  Puntero que recibe los datos recuperados
	 *  @param record_size Tama�o del registro a recuperar
	 *  @param pos Puntero con la posici�n de la que leer y que recibe la nueva posici�n actualizada
	 *  @return N�mero de bytes escritos. Debe coincidir con record_size
	 */
	int32_t writeRecordSet(int32_t recordset, void* data, uint32_t record_size, int32_t* pos);


	/** readRecordSet
	 *  Obtiene un registro de un tama�o desde una posici�n dada, en un recordset abierto previamente
	 *  @param recordset Identificador del manejador de registros
	 *  @param data  Puntero que recibe los datos recuperados
	 *  @param record_size Tama�o del registro a recuperar
	 *  @param pos Puntero con la posici�n de la que leer y que recibe la nueva posici�n actualizada
	 *  @return N�mero de bytes escritos. Debe coincidir con record_size
	 */
	int32_t readRecordSet(int32_t recordset, void* data, uint32_t record_size, int32_t* pos);


	/** getRecord
	 *  Recupera un registro de un tama�o desde una posici�n dada. El recordset se abre y se cierra internamente
	 *  @param data_id Identificador de los datos a recuperar
	 *  @param data  Puntero que recibe los datos recuperados
	 *  @param record_size Tama�o del registro a recuperar
	 *  @param pos Puntero con la posici�n de la que leer y que recibe la nueva posici�n actualizada
	 *  @return N�mero de bytes le�dos. Debe coincidir con record_size
	 */
	int32_t getRecord(const char* data_id, void* data, uint32_t record_size, int32_t* pos);


	/** setRecord
	 *  Escribe un registro de un tama�o desde una posici�n dada
	 *  @param data_id Identificador de los datos a grabar
	 *  @param data  Puntero que recibe los datos recuperados
	 *  @param record_size Tama�o del registro a recuperar
	 *  @param pos Puntero con la posici�n de la que leer y que recibe la nueva posici�n actualizada
	 *  @return N�mero de bytes escritos. Debe coincidir con record_size
	 */
	int32_t setRecord(const char* data_id, void* data, uint32_t record_size, int32_t* pos);

	#endif

protected:

	/** Flag para habilitar trazas de depuraci�n por defecto */
	bool _defdbg;

	/** Mutex de acceso al sistema NVS */
	Mutex _mtx;

private:

	/** Propiedades heredadas de NVSInterface */
	// const char* _name;          /// Nombre del sistema de ficheros
	// int _error;                 /// �ltimo error registrado

	#if __MBED__ == 1
	SPIFBlockDevice* _bd;       /// BlockDevice implementado
	#endif
	#if ESP_PLATFORM == 1
	nvs_handle _handle;
	#endif

	/** Flag para indicar el estado del componente */
	bool _ready;

};
     
#endif /*__FSManager__H */

/**** END OF FILE ****/


