# Dense-Matrix-Multiplication
+ host 코드(C++) : PCIe를 통해 alveo u250 FPGA와 통신, Xilinx XRT API를 이용하여 호스트와 FPGA 커널을 연결
+ kernel 코드(HLS 기반) : Dense Matrix Multiplication 연산을 가속

<br/>

### Block Matrix Multiplication
![Image](https://github.com/user-attachments/assets/06b9a9ad-59d6-4b16-b4ef-f5517e584b44)

<br/>

+ **case 1)**    행렬 데이터가 local BRAM에 저장되지 않고 바로 DRAM으로부터 가져올 때 : 행렬 곱셈에서 각 원소가 여러번 쓰이므로 중복해서 read/write 발생, DRAM transfer size가 커짐. <br/>
+ **case 2)**    행렬 전체를 local BRAM으로 가져올 때 : DRAM transfer 최소화, 그러나 NxN 행렬 전부 가져와서 병렬로 처리하면 resource 사용량 대폭 증가. <br/>

   **-->  행렬을 MxM 단위의 block으로 쪼개서 block 단위로 local BRAM에 읽어와서 프로세싱.**

  <br/><br/>
  ### Loop Ordering
  행렬 곱셈은 결합 법칙이 성립하므로, loop의 i,j,k의 순서를 바꿔도 결과는 똑같음을 이용
  </br>
![Image](https://github.com/user-attachments/assets/9b8c9656-5bde-4c42-8b7b-52b46de19787)

<br/>

+ i -> j -> k: A의 경우 한 line만 local 메모리에 가져올 수 있으나, B의 경우 각 column이 M번 재사용되므로 MxM 전부 local 메모리로 가져와야 함.
+ k -> j -> i: A의 column(a11 ~ a41)과 B의 b11의 M번 곱셈이 일어남, A는 한 column만 local 메모리에, **B의 경우는 원소 하나씩만 local 메모리에 가져올 수 있음**
+ ***k -> i -> j***: A의 a11과 B의 row(b11 ~ b14)의 M번 곱셈이 일어남, B는 한 row만 local 메모리에, **A의 경우는 원소 하나씩만 local 메모리에 가져올 수 있음**

</br>

k -> j -> i와 k -> i -> j의 local BRAM 사용량은 동일하지만, k -> i -> j의 경우 B의 한 row를 읽어오고 AB에 write할 때 DRAM burst transfer를 이용할 수 있으므로 ***k -> i -> j*** 순서 사용
<br/><br/>
### GOPs(Giga Operations Per second)
초당 기가 연산을 의미하며, 보통 CPU,FPGA와 같은 하드웨어 성능 평가에 많이 쓰이는 컴퓨팅 단위
<br/>

+ 행렬 곱셈에서의 GOPs :
  $ GOPs = 2*matrix_size^3 / execution time(ns)
  
