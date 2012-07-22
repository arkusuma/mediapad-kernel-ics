/*add for configuration of tpa2028d1*/
#ifndef TPA2028D1_DATA_H
#define TPA2028D1_DATA_H

unsigned char tpa2028_datas[][7] = {
//     reg1   reg2   reg3  reg4  reg5   reg6  reg7
//pop			reg2(0x01-0x03), reg3(0x06-0x0a) 
	{0x00, 0x02, 0x08, 0x0a, 0x06, 0x3c, 0xc2}, 
//classic			reg2(0x01-0x03), reg3(0x06-0x0a) 
	{0x01, 0x02, 0x07, 0x0a, 0x06, 0x3d, 0xc1}, 
//jazz			reg2(0x01-0x03), reg3(0x06-0x0a) 
	{0x02, 0x06, 0x14, 0x00, 0x06, 0x3d, 0xc1}, 
//rap/hip-hop	reg2(0x01-0x03), reg3(0x06-0x0a) 
	{0x03, 0x02, 0x0a, 0x00, 0x06, 0x3c, 0xc2}, 
//rock			reg2(0x01-0x03), reg3(0x06-0x0a) 
	{0x04, 0x03, 0x19, 0x00, 0x06, 0x3d, 0xc1}, 
//voice/news		reg2(0x01-0x03), reg3(0x06-0x0a) 
	{0x05, 0x02, 0x0a, 0x00, 0x06, 0x3e, 0xc2}, 

};

#endif