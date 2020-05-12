/*
 * FATInterface.h
 *
 *  Created on: Nov 2019
 *      Author: LJM
 *
 *	FATInterface provides FAT portable interface in C++
 *
 */
 
#ifndef __FATInterface__H
#define __FATInterface__H

#include "mbed.h"
#if ESP_PLATFORM == 1
#include "esp_vfs_fat.h"
#endif
#include <list>
#include <dirent.h>

#define DEFAULT_FATInterface_Partition	(const char*)"fat_stm32"
#define MAX_PATH_NAME_LENGTH		50		//Longitud maxima para el path raiz de la particion FAT y partition_label del partition_table



class FATInterface{
  public:

//    struct FATInfo{
//    	char path[MAX_PATH_NAME_LENGTH];
//    	char partition_lable[MAX_PATH_NAME_LENGTH];
//    	bool mounted;
//    };

    FATInterface(const char *partition_label, const char *path, int num_files_max,bool format);
    virtual ~FATInterface();

    static FATInterface* getStaticInstance(){ return _static_instance; }
    bool isReady(){return _ready;};
    //const char* getName(){return _name;};
    //bool isMounted(){return _mounted;};

    void setLoggingLevel(esp_log_level_t level);
    int mount(bool format);
    int umount();
    FILE * open(const char *filename,const char *opentype);
    int close(FILE *stream);
    int _unlink(const char *filename);
    size_t write(const void *data,size_t size,size_t count,FILE*stream);
    size_t read(void *data,size_t size, size_t count,FILE *stream);
    size_t readLine(char* result, size_t max_len, FILE *stream);

    /**
     * Lista los archivos de un directorio y los devuelve como una lista de nombres
     * @param folder Directorio en el que buscar
     * @param file_list Lista a rellenar con los nombres de archivo encontrados
     * @return Número de archivos encontrados
     */
    int listFolder(const char* folder, std::list<const char*> *file_list);

    /**
     * Crea un directorio en disco
     * @param folder Directorio a crear
     * @return 0=OK
     */
    int createFolder(const char* folder);

    /**
     * Copia un fichero en otro
     * @param src Origen
     * @param dest Destino
     * @param erase_src Flag para borrar o no el archivo origen
     * @return 0=OK
     */
    int copyFile(const char* src, const char* dest, bool erase_src);

    /**
     * Elimina el archivo
     * @param file Archivo a eliminar
     * @return 0=OK
     */
    int eraseFile(const char* file);

    /**
     * Chequea si el archivo existe
     * @param file Archivo
     * @return true, false
     */
    bool fileExists(const char* file);

    char * Get_Fat_path(){return _path;};
    char * Get_Fat_label(){return _label;};

  protected:

    //const char* _name;          /* Nombre del sistema de ficheros */
    int _error;                 /* Último error registrado */
    bool _ready;

    bool _defdbg;
    Mutex _mtx;					/* Mutex de acceso al sistema FAT */
    wl_handle_t s_wl_handle;	/* Weat levelling handle */

    char _path[MAX_PATH_NAME_LENGTH];
	char _label[MAX_PATH_NAME_LENGTH];
	int _num_files_max;
	//bool _mounted;

	static FATInterface* _static_instance;


};
     
#endif /*__FATInterface__H */

/**** END OF FILE ****/


