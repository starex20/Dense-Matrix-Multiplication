#include "hls_vector.h"
#include "hls_stream.h"
#include "ap_int.h"
#include "mm.h"
#include <stdio.h>

const int DSIZE = 64/sizeof(DTYPE); // = 32

static void ReadAt(hls::vector<DTYPE, DSIZE>* A, 
                       hls::stream<hls::vector<DTYPE, DSIZE> >& AStreamWide,
                       int N) {
mem_rd:
   for (int ib = 0; ib < N/M; ib++) {
		for (int jb = 0; jb < N/M; jb++) {
			for (int kb = 0; kb < N/M; kb++) {
				for (int k = 0; k < M; k++) {
#pragma HLS pipeline II = 1 
					for (int i = 0; i < M/DSIZE; i++) {
#pragma HLS unroll
						AStreamWide << A[((kb*M+k)*N+ib*M)/DSIZE + i]; // transpose
					}
				}
			}
        }
   }

}

static void ChangeA_Rate(hls::stream<hls::vector<DTYPE, DSIZE> >& AStreamWide,
                       hls::stream<DTYPE> & AStream,
                       int N) {

    for (int ib = 0; ib < N/M; ib++) {
		for (int jb = 0; jb < N/M; jb++) {
			for (int kb = 0; kb < N/M; kb++) {
				for (int k = 0; k < M; k++) {
					for (int ii = 0; ii < M/DSIZE; ii++) {
						hls::vector<DTYPE, DSIZE> A_temp = AStreamWide.read();
						for (int i = 0; i < DSIZE; i++) {
#pragma HLS pipeline II = 1
							AStream.write(A_temp[i]);
						}
					}
				}
			}
		}
    }
}

static void ReadB(hls::vector<DTYPE, DSIZE>* B,  
                       hls::stream<hls::vector<DTYPE, DSIZE> >& BStream, // pragma X ?
                       int N) {
mem_rd:
    for (int ib = 0; ib < N/M; ib++) {  
		for (int jb = 0; jb < N/M; jb++) {
			for (int kb = 0; kb < N/M; kb++) {
				for (int k = 0; k < M; k++) {
#pragma HLS pipeline II = 1
					for (int i = 0; i < M/DSIZE; i++) {
#pragma HLS unroll
						BStream << B[((kb*M+k)*N+jb*M)/DSIZE+i];
					}
				}
			}
		}
    }

}


static void Comp(hls::stream<DTYPE> & AStream,  hls::stream<hls::vector<DTYPE, DSIZE> >& BStream, hls::stream<hls::vector<DTYPE, DSIZE> >& ABStream,  int N )
{

	DTYPE AB_block[M][M];
#pragma HLS ARRAY_PARTITION dim=2 type=complete variable=AB_block

	DTYPE Bj[M];
#pragma HLS ARRAY_PARTITION dim=1 type=complete variable=Bj

	ib_loop: for(int ib = 0; ib < N/M; ib++) {
		jb_loop: for(int jb = 0; jb < N/M; jb++) {
			init_i_loop: for(int i = 0; i < M; i++) {
#pragma HLS pipeline II = 1
				init_j_loop: for(int j = 0; j < M; j++) {
#pragma HLS unroll
					AB_block[i][j] = 0;
				}
			}

			kb_loop: for(int kb = 0; kb < N/M; kb++) {
				k_loop: for(int k = 0; k < M; k++) {
				readB_j_loop: for(int jj = 0; jj < M/DSIZE; jj++) {
#pragma HLS pipeline II = 1
						hls::vector<DTYPE, DSIZE> B_temp = BStream.read(); // = B[((kb*M+k)*N+jb*M)+jj];
						for(int j=0; j < DSIZE; j++) {                                        
#pragma HLS unroll
							Bj[jj*DSIZE + j] = B_temp[j];
						}
					}
/*
					DTYPE A_line[M];
					for (int i=0; i < M; i++) {
#pragma HLS unroll	// stream은 FIFO기 때문에 의미 X
						A_line[i] = AStream.read(); // = A[((kb*M+k)*N+ib*M) + i];
					}
*/

					i_loop: for(int i = 0; i < M; i++) {
#pragma HLS pipeline II=1
						A_line[i] = AStream.read();
						j_loop: for(int j = 0; j < M; j++) {
#pragma HLS unroll
							AB_block[i][j] += A_line[i] * Bj[j];
						}
					}
				}
			}
			

			writeAB_i_loop: for(int i = 0; i < M; i++) {
				writeAB_j_loop: for(int jj = 0; jj < M/DSIZE; jj++) {
#pragma HLS pipeline II = 1
					hls::vector<DTYPE, DSIZE> AB_temp;
					for (int j = 0; j < DSIZE; j++) {
#pragma HLS unroll
						AB_temp[j] = AB_block[i][jj*DSIZE + j];
					}
					//AB[((ib*M+i)*N+jb*M)/DSIZE+jj] = AB_block[i][j];
                    ABStream << AB_temp;
				}
			}
		}
	}
}


static void WriteAB(hls::stream<hls::vector<DTYPE, DSIZE> >& ABStream,  
                       hls::vector<DTYPE, DSIZE> *AB, // pragma X ?
                       int N) {
mem_rd:
    for (int ib = 0; ib < N/M; ib++) {  
		for (int jb = 0; jb < N/M; jb++) {
			for (int i = 0; i < M; i++) {
#pragma HLS pipeline II = 1
				for (int jj = 0; jj < M/DSIZE; jj++) {
#pragma HLS unroll
					AB[((ib*M+i)*N+jb*M)/DSIZE+jj] = ABStream.read();
				}
			}
		}
    }

}


extern "C" {

void vadd(hls::vector<DTYPE, DSIZE> *At, hls::vector<DTYPE, DSIZE> *B, 
				hls::vector<DTYPE, DSIZE> *AB, int M)
{
#pragma HLS INTERFACE mode=m_axi bundle=m0 port=At
#pragma HLS INTERFACE mode=m_axi bundle=m1 port=B
#pragma HLS INTERFACE mode=m_axi bundle=m1 port=AB

#pragma HLS DATAFLOW

// FIFO interface between functions
	hls::stream< hls::vector<DTYPE, DSIZE> > AStreamWide("AStreamWide");
	hls::stream<DTYPE> AStream("AStream");
	hls::stream< hls::vector<DTYPE, DSIZE> > BStream("BStream");
	hls::stream< hls::vector<DTYPE, DSIZE> > ABStream("ABStream");

	ReadAt(At, AStreamWide, M);
	ChangeA_Rate(AStreamWide, AStream, M);
	ReadB(B, BStream, M);
	Comp(AStream, BStream, ABStream, M);
	WriteAB(ABStream, AB, M);


}

}
