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

#define DEFAULT_FATInterface_Partition	(const char*)"fat_stm32"
#define MAX_PATH_NAME_LENGTH		50		//Longitud maxima para el path raiz de la particion FAT y partition_label del partition_table



class FATInterface{
  public:

//    struct FATInfo{
//    	char path[MAX_PATH_NAME_LENGTH];
//    	char partition_lable[MAX_PATH_NAME_LENGTH];
//    	bool mounted;
//    };

    FATInterface(const char *partition_label, const char *path, int num_files_max);
    virtual ~FATInterface();

    static FATInterface* getStaticInstance(){ return _static_instance; }
    bool isReady(){return _ready;};
    //const char* getName(){return _name;};
    //bool isMounted(){return _mounted;};

    int mount();
    int umount();
    FILE * open(const char *filename,const char *opentype);
    int close(FILE *stream);
    size_t write(const void *data,size_t size,size_t count,FILE*stream);
    size_t read(void *data,size_t size, size_t count,FILE *stream);

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


