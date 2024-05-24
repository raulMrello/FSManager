idf_component_register(	SRCS "FATInterface.cpp" 
							"FSManager.cpp"
						INCLUDE_DIRS "."
						REQUIRES fatfs vfs sdmmc mbed_api_esp32)