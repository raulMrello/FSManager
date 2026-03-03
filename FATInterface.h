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

    // Wrappers stdio para evitar que los módulos llamen directamente a fseek/ftell/etc.
    // (y poder ejecutar dichas operaciones desde un hilo con stack en RAM interna).
    int seek(FILE* stream, long offset, int whence);
    long tell(FILE* stream);
    int rewindFile(FILE* stream);
    int flush(FILE* stream);

    size_t readLine(char* result, size_t max_len, FILE *stream);
    size_t getLineCount(FILE *stream);

    /**
     * Lista los archivos de un directorio y los devuelve como una lista de nombres
     * @param folder Directorio en el que buscar
     * @param file_list Lista a rellenar con los nombres de archivo encontrados
     * @return N�mero de archivos encontrados
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
	* Renombra un fichero
	* @param src Origen
	* @param dest Destino
	* @return 0=OK
	*/
    int renameFile(const char* src_file, const char* dest_file);

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

    /*
     * Formatea la particion
     * @return true|false
     * */
    bool format();

  protected:

    //const char* _name;          /* Nombre del sistema de ficheros */
    int _error;                 /* �ltimo error registrado */
    bool _ready;

    bool _defdbg;
    Mutex _mtx;					/* Mutex de acceso al sistema FAT */
    wl_handle_t s_wl_handle;	/* Weat levelling handle */

    char _path[MAX_PATH_NAME_LENGTH];
	char _label[MAX_PATH_NAME_LENGTH];
	int _num_files_max;
	//bool _mounted;

	static FATInterface* _static_instance;

  #if ESP_PLATFORM == 1
  // ---------------------------------------------------------------------------------
  // Worker interno: todas las llamadas a FAT/VFS se ejecutan en este hilo
  // (stack en RAM interna) para que los hilos de ActiveModule puedan ir
  // opcionalmente a memoria externa sin arrastrar las restricciones de flash/FAT.
  // ---------------------------------------------------------------------------------
  enum class WorkerOp : uint8_t {
    Mount,
    Umount,
    Open,
    Close,
    Unlink,
    Write,
    Read,
    Seek,
    Tell,
    Rewind,
    Flush,
    ReadLine,
    GetLineCount,
    ListFolder,
    CreateFolder,
    CopyFile,
    RenameFile,
    EraseFile,
    FileExists,
    Format,
    Stop
  };

  struct WorkerJob {
    WorkerOp op;

    // argumentos comunes
    const char* filename;
    const char* opentype;
    const char* src;
    const char* dest;
    bool flag;

    FILE* stream;
    const void* wdata;
    void* rdata;
    size_t size;
    size_t count;

    long offset;
    int whence;
    char* line_buf;
    size_t line_max;
    std::list<const char*>* file_list;
    const char* folder;

    // resultados
    int result_i;
    bool result_b;
    size_t result_sz;
    long result_l;
    FILE* result_fp;

    Semaphore done;

    explicit WorkerJob(WorkerOp op_) :
      op(op_),
      filename(NULL),
      opentype(NULL),
      src(NULL),
      dest(NULL),
      flag(false),
      stream(NULL),
      wdata(NULL),
      rdata(NULL),
      size(0),
      count(0),
      offset(0),
      whence(0),
      line_buf(NULL),
      line_max(0),
      file_list(NULL),
      folder(NULL),
      result_i(0),
      result_b(false),
      result_sz(0),
      result_l(0),
      result_fp(NULL),
      done(0, 1) {
    }
  };

  static constexpr uint32_t WorkerQueueDepth = 16;
  Thread* _worker_th = NULL;
  Semaphore _worker_started{0, 1};
  Semaphore _worker_stopped{0, 1};
  Queue<WorkerJob, WorkerQueueDepth> _worker_queue;
  osThreadId _worker_tid = NULL;
  bool _worker_ok = false;

  void _ensureWorker();
  bool _inWorkerContext() const;
  void _workerTask();
  void _dispatchJob(WorkerJob& job);

  int _mount_internal(bool format);
  int _umount_internal();
  FILE* _open_internal(const char *filename,const char *opentype);
  int _close_internal(FILE *stream);
  int _unlink_internal(const char *filename);
  size_t _write_internal(const void *data,size_t size,size_t count,FILE*stream);
  size_t _read_internal(void *data,size_t size, size_t count,FILE *stream);
  int _seek_internal(FILE* stream, long offset, int whence);
  long _tell_internal(FILE* stream);
  int _rewind_internal(FILE* stream);
  int _flush_internal(FILE* stream);
  size_t _readLine_internal(char* result, size_t max_len, FILE *stream);
  size_t _getLineCount_internal(FILE *stream);
  int _listFolder_internal(const char* folder, std::list<const char*> *file_list);
  int _createFolder_internal(const char* folder);
  int _copyFile_internal(const char* src, const char* dest, bool erase_src);
  int _renameFile_internal(const char* src_file, const char* dest_file);
  int _eraseFile_internal(const char* file);
  bool _fileExists_internal(const char* file);
  bool _format_internal();
  #endif


};
     
#endif /*__FATInterface__H */

/**** END OF FILE ****/


