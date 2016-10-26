# Phoenix
Software-based power collection tool for NVIDIA and Intel architectures. 

## Options
With Phoenix, one can retrieve power levels of NVIDIA GPUs with fine granularity.

In order to use it, include "phoenix.h" file in your source code. Then, enabling following preprocessor variables will enable different parts of the code. You could do it on the compile time (using -D... flag) or by defining in your source code right before including the file.

### DEBUG_MODE
Enables debug mode on some functions. It just prints out some debugging information.

### PHOENIX_POWER_NVML_API
Enable using NVML library to collect power level. **Remember:** The path to header file should be added to compile command during compilation time. 

### PHOENIX_POWER_NVIDIA_SMI_TOOL
Exploiting power level using *nvidia-smi* tool. Instead of using NVML library, we can run *nvidia-smi* command and ask for power level. 

### PHOENIX_POWER_RAPL_API
<Not fully supported yet.>


## How to use Phoenix?
1. Include the header file, "phoenix.h".

	#include "phoenix.h"

2. In the main function, call following *function* once as one of your first few statements. It is used to initialize the Pheonix.

    PHOENIX_INITIALIZE();

3. Call following function right before the code region you are focusing on with a *region id* and *region name* to start collecting data on it. **Note:** Region id should not exceed *PHOENIX_MAX_SUPPORTED_REGIONS* as specified in the Phoenix header file. **Note 2:** They are mandatory and should be specified. However, you can specify their values based on your requirements. **Note 3:** *region_id* is an integer and *region_name* is a string.

    PHOENIX_ENERGY_TIME_START(region_id, region_name);

Example:

    PHOENIX_ENERGY_TIME_START(1, "region_compute");

4. Call following function right after the code region you are focusing on with *the* region id and region name to stop collecting data on it. 

    PHOENIX_ENERGY_TIME_STOP(region_id, region_name);

Example:

	PHOENIX_ENERGY_TIME_STOP(1, "region_compute");


5. When program is finished, the results are available in a CSV fortmat in "sample.csv" file.

6. Please note: regions could not be nested! Currently, nested regions are not supported.



## Compilation
To compile with Phoenix, you need to compile your code with following flags:

```
   nvcc ... * -DPHOENIX_POWER_NVML_API -I$(NVML_PATH) -I$(PHEONIX_PATH) * ...
```


## Linking
And, you can link your code with following options on NVCC:

```
   nvcc ... * -L/usr/local/cuda-7.5/lib64/ -lnvidia-ml -lm * ...
```


## Output format
The output of Pheonix is saved in "sample.csv" file (unless the user changes it in the header file.)

Here is an example of it when using PHOENIX_POWER_NVML_API. Each line is a collected sample at different times. 

phoenix_region_id,phoenix_region_name,counter,time_us,p_mw
1,region_compute,1,1477496381117441.000,109844.000
1,region_compute,1,1477496381117778.000,109844.000
1,region_compute,1,1477496381118122.000,109844.000
1,region_compute,1,1477496381118275.000,109844.000
1,region_compute,2,1477496381118453.000,109844.000
1,region_compute,2,1477496381118777.000,109844.000
1,region_compute,2,1477496381119117.000,109844.000
...


Details on the columns:

### First and second column: 
the region id and name of the piece of code that we are focusing on. 

### Counter: 
the visiting counter. Any specific region could be visited by our code multiple times. This column shows this phenomenon. For instance, for above data, it shows that region "region_compute" is visited twice. For the first time, 4 samples were collected. And for the second time, 3 samples were collected.

### Time (micro second): 
the timestamp in microsecond. It was extracted using *gettimeoftheday* method in C.

### Power (milli watts): 
the power level at that timestamp collected from the GPU.



By processing this file and integrating power over time, we can extract the consumed energy. By dividing the consumed energy to total execution time, average power consumption is computed. 

