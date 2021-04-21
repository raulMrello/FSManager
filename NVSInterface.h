/*
 * NVSInterface.h
 *
 *  Created on: Ene 2018
 *      Author: raulMrello
 *
 *	NVSInterface provides non-volatile key-value backup portable interface in C++
 *
 */
 
#ifndef __NVSInterface__H
#define __NVSInterface__H

#include "mbed.h"

#define DEFAULT_NVSInterface_Partition	(const char*)"nvs_key"

class NVSInterface{
  public:

	/** KeyValueType
	 * 	Tipo de datos almacenables en el sistema KEY-VALUE
	 */
	enum KeyValueType{
		TypeUint8, //!< TypeUint8
		TypeInt8,  //!< TypeInt8
		TypeUint16,//!< TypeUint16
		TypeInt16, //!< TypeInt16
		TypeUint32,//!< TypeUint32
		TypeInt32, //!< TypeInt32
		TypeUint64,//!< TypeUint64
		TypeInt64, //!< TypeInt64
		TypeString,//!< TypeString
		TypeBlob   //!< TypeBlob
	};
              
    /** Constructor
     *  Crea el gestor del sistema NVS asociando un nombre
     *  @param name Nombre del sistema de ficheros
     */
    NVSInterface(const char *name) : _name(name), _error(0) {
    }
    virtual ~NVSInterface(){}
  
  
    /** init
     *  Inicializa el sistema de ficheros
     *  @return 0 (correcto), <0 (código de error)
     */
    virtual int init() = 0;  
  
    /** ready
     *  Chequea si el sistema de ficheros está listo
     *  @return True (si tiene formato) o False (si tiene errores)
     */
    virtual bool ready() = 0;
  
    /** getName
     *  Obtiene el nombre del sistema de ficheros
     *  @return _name Nombre asignado
     */
    const char* getName() { return _name; }


    /** Abre el handle para realizar varias operaciones en bloque
     *
     * @return True: Handle abierto, False: Handle no abierto (error)
     */
    virtual bool open() = 0;


    /** cierra el handle
     *
     */
    virtual void close() = 0;


    /** save
     *  Graba datos en memoria no volátil de acuerdo a un identificador dado
     *  @param data_id Identificador de los datos a grabar
     *  @param data  Puntero a los datos 
     *  @param size Tamaño de los datos en bytes
     *  @param type tipo de dato
     *  @return Número de bytes escritos
     */   
    virtual int save(const char* data_id, void* data, uint32_t size, KeyValueType type) = 0;
  
  
    /** restore
     *  Recupera datos de memoria no volátil de acuerdo a un identificador dado
     *  @param data_id Identificador de los datos a grabar
     *  @param data  Puntero que recibe los datos recuperados
     *  @param size Tamaño máximo de datos a recuperar
     *  @param type tipo de dato
     *  @return Número de bytes leídos.
     */   
    virtual int restore(const char* data_id, void* data, uint32_t size, KeyValueType type) = 0;


    /** checkKey
     *  Chequea si una clave existe
     *  @param data_id Identificador de la clave
     *  @return true: existe
     */
    virtual bool checkKey(const char* data_id) = 0;

    
    /** removeKey
     *  Elimina una clave
     *  @param data_id Identificador de la clave
     *  @return código de error
     */
    virtual int removeKey(const char* data_id) = 0;

    /** erase
     * Borra la particion
     * @return true|false
     * */
    virtual bool erase() = 0;
  protected:

    const char* _name;          /// Nombre del sistema de ficheros
    int _error;                 /// Último error registrado
};
     
#endif /*__NVSInterface__H */

/**** END OF FILE ****/


