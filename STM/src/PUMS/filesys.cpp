#include "filesys.h"

FATFS   fatfs; 		/* FAT File System */
FRESULT fresult;  /* FAT File Result */
FIL		myfile;		/* FILE Instance */
DIR dir;
FILINFO fno;

char str_dct[256];
const char* path;

UINT BytesWritten;
uint8_t log_file_name[10]="xtx.txt";

RNG_HandleTypeDef hrng;
volatile uint32_t rng_result;
float uint32_to_float;
float l_timestamp;
char buffer_to_sd[150];

char *dec32(unsigned long i) {
	static char str[16];
	char *s = str + sizeof(str);

	*--s = 0;

	do {
		*--s = '0' + (char) (i % 10);
		i /= 10;
	} while (i);

	return (s);
}

void do_sd_logging(void) {
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		fresult = f_write(&myfile, buffer_to_sd, strlen(buffer_to_sd), &BytesWritten);
		printf("STR:%s\n", strcat(buffer_to_sd,"\0"));
		f_sync(&myfile);
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
}

uint8_t	init_log_sd(void) {
		if(board.SD.GetCardState() != HAL_SD_CARD_ERROR)
		{
		printf("init SD card succeed!\n\r");
		fresult = f_mount(&fatfs,"",1);
	
		if (fresult != FR_OK) {
			errorMSG("f_mount error");
		}
		printf("Mount SD card successfully\n\r");

		fresult = f_open(&myfile, "DIR_LIST.TXT", FA_CREATE_ALWAYS | FA_WRITE);

		if (fresult == FR_OK) {
			UINT BytesWritten2;

			path = "";
			fresult = f_opendir(&dir, path);

			if (fresult == FR_OK) {
				while (1) {
					
					char *s = str_dct;
					char *fn;

					fresult = f_readdir(&dir, &fno);

					if ((fresult != FR_OK) || (fno.fname[0] == 0))
						break;

					fn = fno.fname;

					*s++ = ((fno.fattrib & AM_DIR) ? 'D' : '-');
					*s++ = ((fno.fattrib & AM_RDO) ? 'R' : '-');
					*s++ = ((fno.fattrib & AM_SYS) ? 'S' : '-');
					*s++ = ((fno.fattrib & AM_HID) ? 'H' : '-');

					*s++ = ' ';

					strcpy(s, dec32(fno.fsize));
					s += strlen(s);

					*s++ = ' ';

					strcpy(s, path);
					s += strlen(s);

					*s++ = '/';

					strcpy(s, fn);
					s += strlen(s);

					*s++ = 0x0D;
					*s++ = 0x0A;
					*s++ = 0;

					fresult = f_write(&myfile, str_dct, strlen(str_dct), &BytesWritten2);
					printf("%s", str_dct);
				}
			}
			/*	
				Save file
			*/
			f_sync(&myfile);		
			fresult = f_close(&myfile); // DIR.TXT
		}
		return 0;
	}
	else {
		printf("init SD card failed!\n\r");
		return 1;
	}
}

void create_log_file(void) {
		fresult = f_open(&myfile, (const TCHAR*)log_file_name, FA_CREATE_NEW);
		if (fresult != FR_OK) {
			if (fresult == FR_EXIST) {
				printf("File is already exist!\n\r");
				printf("Try to overridely create a new file\n\r");
				fresult = f_open(&myfile, (const TCHAR*)log_file_name, FA_CREATE_ALWAYS|FA_WRITE);
				if (fresult != FR_OK) {
					errorMSG("error in f_open.");
				}
				printf("Override file ok\n\r");
			}
		}
		else {
			printf("Created a new file ok\n\r");
		}
		printf("Ready to log with file:%s\n\r", log_file_name);
		
		char copter_test_str[] = "Dummy\n\r";
		fresult = f_write(&myfile, copter_test_str, strlen(copter_test_str), &BytesWritten);
		f_sync(&myfile);
		/*
			Close here and then open again, in order to fix the file won't save at first
		*/
		f_close(&myfile);
		fresult = f_open(&myfile, (const TCHAR*)log_file_name, FA_OPEN_EXISTING|FA_WRITE);
		
	}