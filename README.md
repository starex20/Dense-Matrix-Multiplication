# Dense-Matrix-Multiplication
+ host 코드(C++) : PCIe를 통해 alveo u250 FPGA와 통신, Xilinx XRT API를 이용하여 호스트와 FPGA 커널을 연결
+ kernel 코드(HLS 기반) : Dense Matrix Multiplication 연산을 가속

<br/>

### Block Matrix Multiplication
![Image](https://github.com/user-attachments/assets/06b9a9ad-59d6-4b16-b4ef-f5517e584b44)

<br/>

case 1) 행렬 데이터가 local BRAM에 저장되지 않고 바로 DRAM으로부터 가져올 때 : 행렬 곱셈에서 행렬의 각 원소가 여러번 쓰이므로 여러번 중복해서 read/write하게 되고 DRAM transfer size가 커짐. <br/>
case 2) A,B 행렬 전체를 local BRAM으로 가져올 때 : DRAM transfer 최소화, 그러나 NxN 행렬 전부 가져오므로 resource 사용량 대폭 증가. <br/><br/>

-->  행렬을 MxM 단위의 block으로 쪼개서 block 단위로 local BRAM에 읽어와서 프로세싱.
