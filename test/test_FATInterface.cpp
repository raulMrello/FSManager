/*
 * test_FATInterface.cpp
 *
 *	Test unitario para el módulo FATInterface
 */



//------------------------------------------------------------------------------------
//-- TEST HEADERS --------------------------------------------------------------------
//------------------------------------------------------------------------------------

#include "unity.h"
#include "FATInterface.h"
#include "mbed.h"
#include "AppConfig.h"
#include "Heap.h"


#if ESP_PLATFORM == 1 || (__MBED__ == 1 && defined(ENABLE_TEST_DEBUGGING) && defined(ENABLE_TEST_FATInterface))

/** Requerido para test unitarios ESP-MDF */
#if ESP_PLATFORM == 1

/** Requerido para test unitarios STM32 */
#elif __MBED__ == 1 && defined(ENABLE_TEST_DEBUGGING) && defined(ENABLE_TEST_FATInterface)
#include "unity_test_runner.h"
#endif


//------------------------------------------------------------------------------------
//-- SPECIFIC COMPONENTS FOR TESTING -------------------------------------------------
//------------------------------------------------------------------------------------

static const char* _MODULE_ = "[TEST_FAT]......";
#define _EXPR_	(true)


//------------------------------------------------------------------------------------
//-- REQUIRED HEADERS & COMPONENTS FOR TESTING ---------------------------------------
//------------------------------------------------------------------------------------

static FATInterface* fat = NULL;

//------------------------------------------------------------------------------------
//-- TEST CASES ----------------------------------------------------------------------
//------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------
TEST_CASE("RESET_______________________", "[FATInterface]") {
	esp_restart();
}


//------------------------------------------------------------------------------------
TEST_CASE("CREA FATINTERFACE ERROR_____", "[FATInterface]") {
	TEST_ASSERT_NULL(fat);
	fat = new FATInterface(FAT_PARTITION_NAME, FAT_PATH, FAT_MAX_FILES, true);
	TEST_ASSERT_NOT_NULL(fat);
	TEST_ASSERT_FALSE(fat->isReady());
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "FAT Filesystem no creado... OK!");
}


//------------------------------------------------------------------------------------
TEST_CASE("CREA FATINTERFACE OK________", "[FATInterface]") {
	TEST_ASSERT_NULL(fat);
	fat = new FATInterface("flash_test", "logs", 2, true);
	TEST_ASSERT_NOT_NULL(fat);
	TEST_ASSERT_TRUE(fat->isReady());
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "FAT Filesystem... OK!");
}



//------------------------------------------------------------------------------------
TEST_CASE("LEE ERRORFILE_______________", "[FATInterface]") {
	TEST_ASSERT_NOT_NULL(fat);
	FILE *f = fat->open(ERROR_FILENAME,"r+");
	TEST_ASSERT_NOT_NULL(f);
	char chunk[128];
	int acc = 0;
	int count=0;
	do{
		count = fat->read(&chunk[acc], sizeof(char), 1, f);
		if(chunk[acc] == '\n' && acc > 0){
			chunk[acc-1] = 0;
			DEBUG_TRACE_D(_EXPR_, _MODULE_, "%s", chunk);
			acc = 0;
		}
		else{
			acc += count;
		}
	}while(count > 0);
	fat->close(f);
}


//------------------------------------------------------------------------------------
TEST_CASE("AÑADE A ERRORFILE___________", "[FATInterface]") {
	TEST_ASSERT_NOT_NULL(fat);
	FILE *f = fat->open(ERROR_FILENAME,"a+");
	TEST_ASSERT_NOT_NULL(f);
	char chunk[128];
	for(int i=0;i<10;i++){
		sprintf(chunk, "Linea %d\r\n", i);
		fat->write(chunk, sizeof(char), strlen(chunk), f);
	}
	fat->close(f);
}





//------------------------------------------------------------------------------------
//-- TEST ENRY POINT -----------------------------------------------------------------
//------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
#if __MBED__ == 1 && defined(ENABLE_TEST_DEBUGGING) && defined(ENABLE_TEST_FATInterface)
void firmwareStart(bool wait_forever){
	esp_log_level_set(_MODULE_, ESP_LOG_DEBUG);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Inicio del programa");
	Heap::setDebugLevel(ESP_LOG_ERROR);
	unity_run_menu();
}
#endif


#endif
