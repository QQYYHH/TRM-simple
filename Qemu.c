/*
    简单模拟器
    模拟CPU运算
    取指
    译码
    执行
    更新pc

    指令集说明如下
    R-type 也就是寄存器操作
    M-type 也就是对内存操作
    J-type 条件跳转
    表达式求值完之后会默认存放在R[0]寄存器

                                                         8    4    0
            |                        |        | +--------+----+----+
mov   rt,rs | R[rt] <- R[rs]         | R-type | |00000000|-rt-|-rs-|
            |                        |        | +--------+----+----+

            |                        |        | +--------+----+----+
add   rt,rs | R[rt] <- R[rs] + R[rt] | R-type | |00000001|-rt-|-rs-|
            |                        |        | +--------+----+----+
            |                        |        | +--------+----+----+
sub   rt,rs | R[rt] <- R[rs] - R[rt] | R-type | |00000010|-rt-|-rs-|
            |                        |        | +--------+----+----+
            |                        |        | +--------+----+----+
mul   rt,rs | R[rt] <- R[rs] * R[rt] | R-type | |00000011|-rt-|-rs-|
            |                        |        | +--------+----+----+
            |                        |        | +--------+----+----+
div   rt,rs | R[rt] <- R[rs] / R[rt] | R-type | |00000100|-rt-|-rs-|
            |                        |        | +--------+----+----+
            |                        |        | +--------+----+----+


load  addr  | R[0] <- M[addr]        | M-type | |00000101|   addr  |
            |                        |        | +--------+----+----+
            |                        |        | +--------+----+----+
store addr  | M[addr] <- R[0]        | M-type | |00000110|   addr  |
            |                        |        | +--------+----+----+

jnz   addr   | R[0] != 0 ,pc -> addr | J-type | |00000111|   addr  |    将条件计算出来的布尔值放入 R[0]，然后判断是否跳转
j     addr  | pc -> addr              | J-type | |00001000|   addr  |  无条件跳转
ret         |  ret                    | E-type  | |00001001|00000000|   结束程序指令

*/

// 规定 前32B 内存放指令(.text)，后32B内存放 数据(.data) 只模拟局部变量，存放在栈区
// 这里为了简化，所有指令的长度一致
#include <stdint.h>
#include <stdio.h>

#define NREG 16
#define NMEM 64

typedef uint16_t word_t; // 字长 16位
// 定义指令格式
// 每条指令 16位
// 冒号后面代表位域，: n，也就是 只开辟低 n 位
// 按照 16 位 寻址
typedef union {
  struct { uint8_t rs : 4, rt : 4, op : 8; } rtype;
  struct { uint8_t addr : 8      , op : 8; } mtype;
  struct { uint8_t addr : 8      , op : 8; } jtype;
  uint16_t inst;
} inst_t;

#define DECODE_R(inst) uint8_t rt = (inst).rtype.rt, rs = (inst).rtype.rs
#define DECODE_M(inst) uint8_t addr = (inst).mtype.addr
#define DECODE_J(inst) uint8_t pc_addr = (inst).jtype.addr

uint8_t pc = 0;       // PC 取指令范围 0-31
uint16_t R[NREG] = {}; // 寄存器 16位
uint16_t M[NMEM] = {   // 内存, 其中包含一个计算z = x + y的程序
  0b0000010100100001, // load M[33]  R[0] <- 100  0
  0b0000011100000011, // jnz R[0] != 0, goto 3    1
  0b0000100000001111, // goto 15                   2
  0b0000010100100000, // load 32, R[0] = M[32]    3
  0b0000000000010000, // R[1] = R[0]              4
  0b0000010100100001, // load 33, R[0] = M[33]    5
  0b0000000100000001, // add 0 1, R[0] = R[0] + R[1]  6
  0b0000011000100000, // store 32 , M[32] = R[0]      7
  0b0000010100100001, // load 33 R[0] = M[33] , R[0] = i;            8
  0b0000000000010000, // R[1] = R[0] ,                               9
  0b0000010100100010, // load 34, R[0] = 1                       10
  0b0000001000010000, // sub 1 0, R[1] = R[1] - R[0]             11
  0b0000000000000001, // R[0] = R[1]                             12
  0b0000011000100001, // store i, M[33] = R[0]                   13
  0b0000100000000001, // goto 1                                  14
  0b0000100100000000, // end                  15
};

int halt = 0; // 结束标志
uint8_t data_pointer = 32; // 初始化数据区（栈区）指针

void store_var_instack(word_t data){
  M[data_pointer++] = data;
}
// 执行一条指令
void exec_once() {
  inst_t this;
  this.inst = M[pc]; // 取指
  uint8_t is_jump = 0;
  switch (this.rtype.op) {
  //  操作码译码       操作数译码           执行
    case 0b00000000: { DECODE_R(this); R[rt]   = R[rs];   break; }
    case 0b00000001: { DECODE_R(this); R[rt]  += R[rs];   break; } // +
    case 0b00000010: { DECODE_R(this); R[rt]  -= R[rs]; break; } // -
    case 0b00000011: { DECODE_R(this); R[rt]  *= R[rs];    break; } // *
    case 0b00000100: { DECODE_R(this); R[rt]  /= R[rs];    break; } // /
    case 0b00000101: { DECODE_M(this); R[0]   =M[addr];   break;} // load
    case 0b00000110: { DECODE_M(this); M[addr]    =R[0];  break;} // store
    case 0b00000111: { 
      DECODE_J(this); 
      if(R[0]){
        pc = pc_addr;
        is_jump = 1;
      }
      break;
    }
    case 0b00001000: {
      DECODE_J(this);
      is_jump = 1;
      pc = pc_addr;
      break;
    }
    default:
      printf("Invalid instruction with opcode = %x, halting...\n", this.rtype.op);
      halt = 1;
      break;
  }
  if(!is_jump) pc ++; // 更新PC
}


int main() {
  // 求解 1 + 2 + ... 100
  store_var_instack(0); // s = 0
  store_var_instack(100); // i = 100
  store_var_instack(1); // incre = 1
  int cnt = 0;
  while (1) {
    cnt++;
    exec_once();
    if (halt) break;
  }
  printf("instruction exec count is: %d \nthe value of \"1 + 2 + ... + 100\" is %d\n", cnt, M[32]);
  return 0;
}