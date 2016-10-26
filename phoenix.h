
#ifndef __PHOENIX_H__
#define __PHOENIX_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>



// #define DEBUG_MODE
// #define PHOENIX_POWER_NVML_API
// #define PHOENIX_POWER_NVIDIA_SMI_TOOL
// #define PHOENIX_POWER_RAPL_API


#if defined(PHOENIX_POWER_NVML_API) || defined(PHOENIX_POWER_NVIDIA_SMI_TOOL)
#define PHOENIX_MONITOR_POWER
#endif

#ifdef PHOENIX_MONITOR_POWER
#if !defined(PHOENIX_POWER_NVML_API) && !defined(PHOENIX_POWER_NVIDIA_SMI_TOOL)
#define PHOENIX_POWER_NVML_API
#endif
#endif


#ifdef PHOENIX_POWER_NVML_API
#include <nvml.h>
#endif


#ifdef DEBUG_MODE
#define __DEBUG_PRINT(str) fprintf(stderr, "\n\n%s" str)
#else
#define __DEBUG_PRINT(str)
#endif




#define PHOENIX_BUFSIZE 							128
#define PHOENIX_MAX_SAMPLE  						(2048*2048)		// 4M samples
#define PHOENIX_MAX_REGION_NAME_LEN					64
#define PHOENIX_MAX_SUPPORTED_REGIONS				32		/* Zero-based counter */

#define PHOENIX_SAMPLE_FILENAME 					"sample.gpu"

#define SAMPLE_TRANSFER_TO_FILE(filename)			{if(phoenix_sample_count >= PHOENIX_MAX_SAMPLE) {transfer_samples_to_file(filename);}}


#define PHOENIX_GPU_SAMPLING_DELAY_IN_US 			1
#define PHOENIX_CPU_SAMPLING_DELAY_IN_MS 			10


static 	char 	phoenix_sample_buffer[PHOENIX_MAX_SAMPLE][PHOENIX_BUFSIZE];
static 	int 	phoenix_sample_count = 0;


static int 		phoenix_control_var = 0;
static int 		phoenix_control_in_sleep = 0;
static int 		phoenix_exit = 0;



static pthread_t 		phoenix_thread;



static char		phoenix_first_row_csv_pattern[PHOENIX_BUFSIZE*2];
static int  	phoenix_region_id;
static char		phoenix_region_name[PHOENIX_MAX_REGION_NAME_LEN];
static int  	phoenix_start_counter[PHOENIX_MAX_SUPPORTED_REGIONS];
static int 		phoenix_stop_counter[PHOENIX_MAX_SUPPORTED_REGIONS];


static struct timeval phoenix_time_tv_start, phoenix_time_tv_stop;



static int file_transfer_counter = 0;
inline void transfer_samples_to_file(char *filename) {
	printf("---===--- PHOENIX - Writting final values to \"%s\"... (%d samples)\n", filename, phoenix_sample_count);

	FILE *f;
	if(file_transfer_counter == 0) {
		f = fopen(filename, "w");
		fwrite(&phoenix_first_row_csv_pattern[0], 1, strlen(phoenix_first_row_csv_pattern), f);
		fwrite("\n", 1, strlen("\n"), f);
	} else {
    	f = fopen(filename, "a");
	}
    file_transfer_counter++;

    int i;
    for(i=0;i<phoenix_sample_count;i++) {
        fwrite(&phoenix_sample_buffer[i][0], 1, strlen(phoenix_sample_buffer[i]), f);
    }
    fclose(f);
    phoenix_sample_count = 0;
}



inline void phoenix_cleanup() {
	phoenix_exit = 1;
	pthread_join(phoenix_thread, NULL);

	transfer_samples_to_file(PHOENIX_SAMPLE_FILENAME);
}


inline void phoenix_time_cleanup() {
	transfer_samples_to_file(PHOENIX_SAMPLE_FILENAME);
}




#ifdef PHOENIX_POWER_NVML_API


void *phoenix_control(void *) {

	unsigned int p;
	int GPUDevId = 0;
	nvmlDevice_t device;
	nvmlReturn_t result;

	/* Initilize NVML library */
	result = nvmlInit();
	if (NVML_SUCCESS != result) {
		fprintf(stderr, "--- NVML: Failed to initialize: %s\n", nvmlErrorString(result));
		exit(0);
	}

	/* Get handle of device */
	result = nvmlDeviceGetHandleByIndex(GPUDevId , &device);
	if (result != NVML_SUCCESS) {
		fprintf(stderr, "--- NVML: Handle can not be initialized: %s\n", nvmlErrorString(result));
		exit(0);
	}

	strcpy(&phoenix_first_row_csv_pattern[0], "phoenix_region_id,phoenix_region_name,counter,time_us,p_mw");

	while(phoenix_exit != 1) {
		if(phoenix_control_var != 1) {
			phoenix_control_in_sleep = 1;
		    usleep(PHOENIX_GPU_SAMPLING_DELAY_IN_US);
		    continue;
		}

		phoenix_control_in_sleep = 0;


		#ifdef DEBUG_MODE
		result = nvmlDeviceGetPowerUsage( device, &p );
		if( result != NVML_SUCCESS) {
			printf( "--- NVML: Something went wrong %s\n", nvmlErrorString(result));
			exit(1);
		}
		#else
		nvmlDeviceGetPowerUsage( device, &p );
		#endif


	    struct timeval  tv;
	    gettimeofday(&tv, NULL);
	    double time_in_micro = (tv.tv_sec) * 1.0E6 + (tv.tv_usec);

	    sprintf(&phoenix_sample_buffer[phoenix_sample_count][0], "%d,%s,%d,%.3f,%.3f\n", phoenix_region_id, &phoenix_region_name[0], phoenix_start_counter[phoenix_region_id], time_in_micro, p*1.0);
	    phoenix_sample_count++;

	    SAMPLE_TRANSFER_TO_FILE(PHOENIX_SAMPLE_FILENAME);



	    usleep(PHOENIX_GPU_SAMPLING_DELAY_IN_US);
	}

	return NULL;
}


#elif defined(POWER_NVIDIA_SMI_API)


void *phoenix_control(void *) {

	strcpy(&phoenix_first_row_csv_pattern[0], "timestamp,fan.speed,pstate,utilization.gpu,utilization.memory,temperature.gpu,power.draw,clocks.sm,clocks.mem");
	char cmd[256];
	sprintf(&cmd[0], "nvidia-smi --query-gpu=%s --format=csv,noheader,nounits", &phoenix_first_row_csv_pattern[0]);
	sprintf(&phoenix_first_row_csv_pattern[0], "phoenix_region_id,phoenix_region_name,counter,timestamp,fan.speed,pstate,utilization.gpu,utilization.memory,temperature.gpu,power.draw,clocks.sm,clocks.mem", &phoenix_first_row_csv_pattern[0]);

    char out[BUFSIZE];

    FILE *fp;

	while(phoenix_exit != 1) {
		if(phoenix_control_var != 1) {
			phoenix_control_in_sleep = 1;
		    usleep(PHOENIX_GPU_SAMPLING_DELAY_IN_US);
		    continue;
		}
		phoenix_control_in_sleep = 0;


	    #ifdef DEBUG_MODE
	    if ((fp = popen(&cmd[0], "r")) == NULL) {
	        perror("Error opening PIPE. ");
	        exit(0);
	    }
	    if (fgets(out, BUFSIZE, fp) == NULL) {
	        perror("Unable to process output of NVidia-SMI. ");
	        exit(0);
	    }
	    if(pclose(fp))  {
	        printf("Command not found or exited with error status\n");
	        exit(0);
	    }
	    #else
	    fp = popen(&cmd[0], "r");
	    fgets(out, BUFSIZE, fp);
	    pclose(fp);
	    #endif

	    usleep(PHOENIX_GPU_SAMPLING_DELAY_IN_US);

	    // Output from SMI has an implicit "\n" character. So, no need to take care of it.
	    sprintf(&phoenix_sample_buffer[phoenix_sample_count][0], "%d,%s,%d,%s", phoenix_region_id, &phoenix_region_name[0], phoenix_start_counter[phoenix_region_id], &out[0]);
	    phoenix_sample_count++;

	    SAMPLE_TRANSFER_TO_FILE(PHOENIX_SAMPLE_FILENAME);
	}

	return NULL;
}



#elif defined(PHOENIX_POWER_RAPL_API)

#include "rapl-wrapper.h"

static double rapl_en0, rapl_en1;

static int core0;
static PETU_t pet0;


void *phoenix_control(void *) {

	int cpu_model = detect_cpu();
	core0 = msr_open(0);
	pet0 = msr_calculate_units(core0);
	PackagePowerInfo_t ppi0 = msr_get_package_power_info(core0, &pet0);

	strcpy(&phoenix_first_row_csv_pattern[0], "phoenix_region_id,phoenix_region_name,counter,time_us,p_mw");

	rapl_en0 = msr_get_package_energy(core0, &pet0);

	while(phoenix_exit != 1) {
		if(phoenix_control_var != 1) {
			phoenix_control_in_sleep = 1;

			rapl_en0 = msr_get_package_energy(core0, &pet0);
		    usleep(PHOENIX_CPU_SAMPLING_DELAY_IN_MS * 1000);

		    continue;
		}

		phoenix_control_in_sleep = 0;


	    struct timeval  tv;
	    gettimeofday(&tv, NULL);
	    double time_in_micro = (tv.tv_sec) * 1.0E6 + (tv.tv_usec);

		rapl_en1 = msr_get_package_energy(core0, &pet0);
		double delta_e = rapl_en1 - rapl_en0;
		rapl_en0 = rapl_en1;
		double p = delta_e / (PHOENIX_CPU_SAMPLING_DELAY_IN_MS * 1E-3);   /* P in mW */

		// printf("---- delta_e: %.3f -        time: %.3f - p: %.6f\n", delta_e, PHOENIX_CPU_SAMPLING_DELAY_IN_MS * 1.0, p * 1000.0);

	    sprintf(&phoenix_sample_buffer[phoenix_sample_count][0], "%d,%s,%d,%.3f,%.3f\n", phoenix_region_id, &phoenix_region_name[0], phoenix_start_counter[phoenix_region_id], time_in_micro, p*1000.0);
	    phoenix_sample_count++;
	    SAMPLE_TRANSFER_TO_FILE(PHOENIX_SAMPLE_FILENAME);


	    usleep(PHOENIX_CPU_SAMPLING_DELAY_IN_MS * 1000);
	}

	return NULL;
}


#else

#warning "\n\n--- PHOENIX - No method was chosen!"

#endif







#ifdef PHOENIX_MONITOR_POWER



void phoenix_init() {
	static int init_once = 0;
	if(init_once == 1) 
		return;
	init_once++;


	int i;
	for(i=0;i<PHOENIX_MAX_SUPPORTED_REGIONS;i++)
		phoenix_start_counter[i] = phoenix_stop_counter[i] = 0;
	atexit(phoenix_cleanup);

	pthread_create(&phoenix_thread, NULL, phoenix_control, NULL);
	usleep(1000);

	printf("\n\n---===--- PHOENIX Started...\n\n");
}

void phoenix_region_start(int id, char *name) {
	__DEBUG_PRINT("PHONENIX --- REGION START\n");

	phoenix_region_id = id;

	strcpy(&phoenix_region_name[0], name);
	phoenix_start_counter[id]++;

	phoenix_control_var = 1;
	while(phoenix_control_in_sleep != 0) {
		// Without this line, the while loop would be executed so fast on one of the cores that leads to starvation on the other cores.
		usleep(50);
	}

	__DEBUG_PRINT("PHONENIX --- REGION START - Done\n");
}


void phoenix_region_stop(int id, char *name) {
	__DEBUG_PRINT("PHONENIX --- REGION STOP\n");

	phoenix_stop_counter[id]++;

	phoenix_control_var = 0;
	while(phoenix_control_in_sleep == 0) {
		// Without this line, the while loop would be executed so fast on one of the cores that leads to starvation on the other cores.
		usleep(50);
	}

	__DEBUG_PRINT("PHONENIX --- REGION STOP - Done\n");
}






void phoenix_time_init() {
	static int init_once = 0;
	if(init_once == 1) 
		return;
	init_once++;


	int i;
	for(i=0;i<PHOENIX_MAX_SUPPORTED_REGIONS;i++)
		phoenix_start_counter[i] = phoenix_stop_counter[i] = 0;
	sprintf(&phoenix_first_row_csv_pattern[0], "phoenix_region_id,phoenix_region_name,counter,time_ms");

	atexit(phoenix_time_cleanup);

	printf("\n\n---===--- PHOENIX (only time) Started...\n\n");
}

inline double phoenix_time_get() {
	return (phoenix_time_tv_stop.tv_sec - phoenix_time_tv_start.tv_sec) * 1.0E6 + (phoenix_time_tv_stop.tv_usec - phoenix_time_tv_start.tv_usec);
}


inline void phoenix_time_start(int id, char *name) {
	phoenix_region_id = id;
	strcpy(&phoenix_region_name[0], name);
	phoenix_start_counter[id]++;
	gettimeofday(&phoenix_time_tv_start, NULL);
}

inline void phoenix_time_stop(int id, char *name) {
	phoenix_stop_counter[id]++;
	gettimeofday(&phoenix_time_tv_stop, NULL);

	sprintf(&phoenix_sample_buffer[phoenix_sample_count][0], "%d,%s,%d,%.3f\n", phoenix_region_id, &phoenix_region_name[0], phoenix_start_counter[phoenix_region_id], phoenix_time_get());
	phoenix_sample_count++;

	SAMPLE_TRANSFER_TO_FILE(PHOENIX_SAMPLE_FILENAME);
}



#endif



#ifdef 	PHOENIX_MONITOR_POWER
#define PHOENIX_ENERGY_TIME_START(phoenix_region_id, phoenix_region_name) phoenix_region_start(phoenix_region_id, phoenix_region_name);
#define PHOENIX_ENERGY_TIME_STOP(phoenix_region_id, phoenix_region_name) phoenix_region_stop(phoenix_region_id, phoenix_region_name);
#elif  	defined(PHOENIX_MONITOR_TIME)
#define PHOENIX_ENERGY_TIME_START(phoenix_region_id, phoenix_region_name) phoenix_time_start(phoenix_region_id, phoenix_region_name);
#define PHOENIX_ENERGY_TIME_STOP(phoenix_region_id, phoenix_region_name) phoenix_time_stop(phoenix_region_id, phoenix_region_name);
#else
#define PHOENIX_ENERGY_TIME_START(phoenix_region_id, phoenix_region_name) {} 
#define PHOENIX_ENERGY_TIME_STOP(phoenix_region_id, phoenix_region_name)  {}
#endif



#ifdef 	PHOENIX_MONITOR_POWER
#define PHOENIX_INITIALIZE()  phoenix_init();
#elif  	defined(PHOENIX_MONITOR_TIME)
#define PHOENIX_INITIALIZE()  phoenix_time_init();
#else
#define PHOENIX_INITIALIZE()  {}
#endif


#endif

