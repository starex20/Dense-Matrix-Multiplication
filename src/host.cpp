#include <algorithm>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "mm.h"
#include <iostream>
#include <cstring>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"


const int SIZE = 512;
//const int SIZE = 2048;

void mm_sw( DTYPE* At, DTYPE* B, std::vector<DTYPE>& AB){

	DTYPE sum = 0;
//#pragma omp parallel for
	for(int i = 0; i < SIZE; i++){
		for(int j = 0; j<SIZE; j++){
			sum = 0;
			for(int k = 0; k < SIZE; k++){
				sum = sum + At[k*SIZE+i] * B[k*SIZE+j];
			}
			AB[i*SIZE+j] = sum;
		}
	}
}

int main(int argc, char** argv) {

    // Read settings
    std::string binaryFile = "mm.xclbin"
    int device_index = 0;

    if (argc < 3) {
        parser.printHelp();
        return EXIT_FAILURE;
    }

    std::cout << "Open the device" << device_index << std::endl;
    auto device = xrt::device(device_index);
    std::cout << "Load the xclbin " << binaryFile << std::endl;
    auto uuid = device.load_xclbin(binaryFile);
    
	//std::vector<DTYPE> At(SIZE*SIZE); 
	//std::vector<DTYPE> B(SIZE*SIZE); 
	std::vector<DTYPE> AB_sw(SIZE*SIZE); 
	//std::vector<DTYPE, aligned_allocator<DTYPE> > AB_hw(SIZE*SIZE); 


    size_t vector_size_bytes = sizeof(DTYPE) * SIZE * SIZE;

    // make kernel;
    auto krnl = xrt::kernel(device, uuid, "mm"); 

    std::cout << "Allocate Buffer in Global Memory\n";
    auto boIn1 = xrt::bo(device, vector_size_bytes, krnl.group_id(0)); //Match kernel arguments to RTL kernel
    auto boIn2 = xrt::bo(device, vector_size_bytes, krnl.group_id(1));
    auto boOut = xrt::bo(device, vector_size_bytes, krnl.group_id(2));

    // Map the contents of the buffer object into host memory
    srand(time(NULL));
    auto At = boIn1.map<DTYPE*>();
    auto B = boIn2.map<DTYPE*>();
    auto AB_hw = boOut.map<DTYPE*>();
    std::fill(At, At + SIZE*SIZE, rand() % 8);
    std::fill(B, B + SIZE*SIZE, rand() % 8);
    std::fill(AB_hw, AB_hw + SIZE*SIZE, 0);

     // Create the test data
    mm_sw(At, B, AB_sw);

     // Synchronize buffer content with device side
    std::cout << "synchronize input buffer data to device global memory\n";
    boIn1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    boIn2.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "Execution of the kernel\n";
    auto start = std::chrono::steady_clock::now();
    auto run = krnl(boIn1, boIn2, boOut, SIZE); //DATA_SIZE=size
    run.wait();    
    auto end = std::chrono::steady_clock::now();
   
    std::cout << "Done.\n";
	double exec_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
	double gops = double(SIZE) * SIZE * SIZE * 2 / (exec_time);
	std::cout << "Time: " << exec_time*1e-9 << ", GOPS: " << gops << std::endl;


    // Get the output;
    std::cout << "Get the output data from the device" << std::endl;
    boOut.sync(XCL_BO_SYNC_BO_FROM_DEVICE);


	int err_cnt = 0;
	for(int i = 0; i<SIZE; i++){
		for(int j = 0; j<SIZE; j++){
			if(AB_sw[i*SIZE+j] != AB_hw[i*SIZE+j]) {
				err_cnt++;
				if( err_cnt == 1 ){
					printf("i:%d j:%d sw:%d hw:%d\n", i, j, AB_sw[i*SIZE+j], AB_hw[i*SIZE+j] );
				}
			}
		}
	}

	if(err_cnt != 0){
		printf("FAILED! Error count : %d\n", err_cnt);
	}
	else{
		printf("PASSED!\n");
	}

	return EXIT_SUCCESS;
}
