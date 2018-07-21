
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#define _CRT_SECURE_NO_WARNINGS
#define NUM 70000000 //记录总条数 70亿条 测试0.7亿
#define INRECORDS 30000000 //内存容纳条数 3亿条 测试0.3亿
#define BLOCKS 5   //分块数 >NUM/INRECORDS   分成5块//默认三位十进制数字
char unsort[30]="D://unsort/unsort.dat";   //待排序文件
char tmpsort[30]="C://tmpsort/tmpsort.dat" ;   //临时文件
char sorted[30]="C://sorted/sorted.dat" ;  //目标文件

char getline[30];
time_t start = clock();
time_t last = start;

/* 公共变量在函数外定义
* unsigned __int64 random()
* struct Record
* int compare(const void *a, const void *b)
* int longcmp(const void *a, const void *b)
* int generate()
* int display(char path[30])
* int sampling()
* int singlesort()
* int assign()
* int merge()
*/

// generate()   //生成记录结构
struct Record {
	unsigned __int64 key;
	unsigned __int64 value;
};
unsigned __int64 random() {  //一个rand()值域至少31位，为了保证随机性，选三个数各取24，20，20位
	unsigned __int64 a, b, c;
	a = rand(); a <<= 40;
	b = rand(); b <<= 44; b >>= 24;
	c = rand(); c <<= 44; c >>= 44;
	return (a + b + c);  //+优先于<<
}
int generate(unsigned __int64 num = NUM) {
	struct Record record;
	FILE * file;
	file = fopen(unsort, "wb");
	if (!file) {
		printf("fail to open %s \n", unsort);
		return 1;
	}
	srand((unsigned)time(NULL));  //随机数种子一次就行
	for (unsigned __int64 i = 0; i<num; i++) {
		record.key = random();
		record.value = random();
		fwrite(&record, sizeof(record), 1, file);
	}
	printf("%I64u records generated\n", num);
	fclose(file);
	return 0;
}

int maketmp(unsigned __int64 num = INRECORDS * BLOCKS) {  //产生临时大文件，两倍于源文件
	struct Record record = { 0,0 };
	FILE * file = fopen(tmpsort, "wb");
	if (!file) {
		printf("fail to open %s \n", tmpsort);
		return 1;
	}
	for (unsigned __int64 i = 0; i<num; i++) {
		fwrite(&record, sizeof(record), 1, file);
	}
	printf("%I64u blank records generated\n", num);
	fclose(file);
	return 0;
}
//display()
int display(char path[30]) {
	struct Record record;
	FILE * file = fopen(path, "rb");
	if (file == NULL) {
		printf("fail read %s!\n", path);
		return 1;
	}
	int index = 0;
	int c;
	while (!feof(file)) {
		c = fread(&record, sizeof(record), 1, file);
		printf("%d %d  %I64u %I64u  \n", c, index, record.key, record.value);
		index++;
	}
	fclose(file);
	return 0;
}

//sampling()  产生outline[]   
unsigned __int64 outline[BLOCKS];  //sampling()反馈结果，根据样本分位数，选出各分块的键
								   //  unsigned long address[BLOCKS*100];  //取随机记录条号
								   //	struct Record sample[BLOCKS*100];  //根据随机地址抽取样本记录
int compare(const void *a, const void *b) {  //定义记录比较函数
	Record* r1 = (Record *)a;
	Record* r2 = (Record *)b;
	__int64 d = (r1->key) - (r2->key);
	int e = (d >= 0 ? 1 : -1);
	//printf("%d %I64d  %I64u %I64u \n",e,d,r1->key,r2->key);
	return e;
}
int longcmp(const void *a, const void *b) {  //定义地址比较函数，只支持long
	unsigned long *r1 = (unsigned long *)a;
	unsigned long *r2 = (unsigned long *)b;
	return (*r1) - (*r2);
}

//缓存分块  BLOCKS个缓存分块，内存可以容纳INRECORDS条记录，每个分块可以在内存中拥有INRECORDS/BLOCKS条记录，以防止波动。
struct block {  //内存中分块记录
	unsigned __int64 key;  //分块的键
	unsigned __int64 instart;  //内存中起始位置
	unsigned int incount;  //内存缓存计数  ，最大是0.8*INRECORDS/BLOCKS
	unsigned __int64 outstart; //在tmport.dat的起始位置  2NUM/BLOCKS*id
	unsigned __int64 outcount;  //外存中的计数  
};
struct block blocklist[BLOCKS];  //归并阶段也要使用此列表  ,分块号id索引  

int sampling(char path[30]=unsort) {  //抽样函数
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		printf("fail read %s \n", path);
		return 1;
	}

	unsigned __int64 address[BLOCKS * 100]; //抽取随机地址
	struct Record sample[BLOCKS * 100];  //抽取样本

	srand((unsigned)time(NULL)); //应该依次生成随机记录条号，便于操作系统一次读取。可以重复，因为记录的key本来可能有重复
	for (int i = 0; i<BLOCKS * 100; i++) {
		address[i] = rand() % 4096 * NUM / 4096;  // 随机地址取记录,2000样本要求概率大于2000
    }

	qsort(address, BLOCKS * 100, sizeof(address[0]), longcmp);   //地址排序，便于硬盘操作
																 //struct Record sample[BLOCKS*100];  //抽取样本
	for (int i = 0; i<BLOCKS * 100; i++) {
		fseek(file, address[i] * sizeof(Record), SEEK_SET);
		fread(&sample[i], sizeof(Record), 1, file);
	}
	fclose(file);

	qsort(sample, BLOCKS * 100, sizeof(Record), compare);  //样本排序
	for (int i = 0; i<BLOCKS; i++) {   //选出键0~BLOCKS-1，最后BLOCK-1个键有用,也就是轮廓1~BLOCK分成BLOCK块
		blocklist[i].key = sample[i * 100].key;
		printf("startkey :%d %I64u \n", i, blocklist[i].key);
	}
	printf("sampling OK! \n");
	return 0;
}
//根据键值求分块号
int getid(unsigned __int64 key) {
	int id = 0;
	if (key >= blocklist[BLOCKS - 1].key) {
		id = BLOCKS - 1;
	}
	else {
		for (id = 0; key >= blocklist[id + 1].key; id++) {}
	}
	return id;
}
//singlesort()  适用于硬盘数量较少情况下
int singlesort(char inpath[30], char outpath[30]) {

	FILE *infile = fopen(inpath, "rb");
	if (infile == NULL) {
		printf("fail read %s \n", inpath);
		return 1;
	}
	FILE *outfile = fopen(outpath, "ab");//不清空文件了，因为多硬盘算法还要用
	if (outfile == NULL) {
		printf("fail write %s \n", outpath);
		return 1;
	}

	//本块的存入list中排序，再写出
	unsigned __int64 outsum = 0;  //统计外存记录总数
	unsigned __int64 index = 0;//list下标，每次循环重置    
	int id = 0;  //分块号游标
	for (id = 0; id<BLOCKS; id++) {
		struct Record record = { 0,0 };
		struct Record *list = (Record*)calloc(INRECORDS, sizeof(Record));
		if (!list) {
			printf("bad list \n");
			return 1;
		}
		index = 0;//list下标，每次循环重置
		fseek(infile, 0, SEEK_SET);
		while (!feof(infile)) {   //每一个循环遍历源文件，装载一个分块到内存
			fread(&record, sizeof(Record), 1, infile);
			if (getid(record.key) == id) {
				list[index] = record;
				index++;
			}
		} //源文件读取完毕，生成本块加载到内存
		blocklist[id].outcount = index;  //最后一次循环加过一。末尾再将index存入计数
		outsum += blocklist[id].outcount;
		printf("block%d : [%I64u ~ %I64u)  counts:%I64u  outsum:%I64u \n", id, blocklist[id].key, blocklist[id + 1].key, blocklist[id].outcount, outsum);
		if (blocklist[id].outcount>INRECORDS) {         //检测count，若大于INRECORDS,则说明分快太大。添加到本分块数组之后再检测
			printf("block%d counts %I64u overflow %I64u limit \n", id, blocklist[id].outcount, INRECORDS);
			return 1;
		}
		qsort(list, blocklist[id].outcount, sizeof(Record), compare);
		//fseek(outfile, 0, SEEK_END);  //追加记录
		fwrite(&list, sizeof(Record), blocklist[id].outcount, outfile); //写入块

		free(list);
	}
	//此时排完所有的0~BLOCKS-1块

	printf("infile:%I64u records  outfile:%I64u records\n", NUM, outsum);
	fclose(infile);
	fclose(outfile);
	printf("sorted!\n");
	return 0;
}

int validate(char path[30]) {
	struct Record record2, record1;
	record1 = { 0,0 };
	record2 = { 0,0 };

	FILE * file = fopen(path, "rb");
	if (file == NULL) {
		printf("fail read %s \n",path);
		return 1;
	}
	unsigned __int64 index = 0;
	while (fread(&record2, sizeof(Record), 1, file)) {   //fread()本身会移动游标
		if ((record2.key - record1.key)<0) {
			printf("fail %I64u %I64u %I64u \n", index, record1.key, record2.key);
			return 1;
		}
		record1 = record2;
		index++;
	}
	fclose(file);
	printf("%s is sorted! \n",path);
	return 0;
}

int assign(char inpath[30], char tmppath[30]) {
	FILE *infile = fopen(inpath, "rb");
	if (infile == NULL) {
		printf("fail read %s \n", inpath);
		return 1;
	}
	FILE *sortingfile = fopen(tmppath, "rb+");  //清空文件
	if (tmpfile == NULL) {
		printf("fail write %s \n", tmppath);
		return 1;
	}
	struct Record record = { 0,0 };
	struct Record *list = (Record*)calloc(INRECORDS, sizeof(Record));  //存入一张大表中，各分块缓存分配[id~(id+1))*(INRECORDS/BLOCKS)地址

	for (int i = 0; i<BLOCKS; i++) {
		blocklist[i].instart = i * (INRECORDS / BLOCKS); 
        blocklist[i].outstart = i * INRECORDS;
		blocklist[i].incount = 0;        
		blocklist[i].outcount = 0;
	}


	while (!feof(infile)) {  //每次循环将一条源文件记录分配至内存缓冲，缓冲若满则写出
		fread(&record, sizeof(Record), 1, infile);
		int id = getid(record.key);  //分块号游标
		list[blocklist[id].instart + blocklist[id].incount] = record;  //在count位置写，写完更新count
		blocklist[id].incount++;   //写内存完成   

		if (blocklist[id].incount > INRECORDS/BLOCKS) {    //满了写出缓存到外存文件
			fseek(sortingfile, sizeof(Record)*id + blocklist[id].outcount, SEEK_SET);
			fwrite(&list[blocklist[id].instart ], sizeof(Record), INRECORDS / BLOCKS, sortingfile);
			blocklist[id].outcount += INRECORDS / BLOCKS;
			blocklist[id].incount = 0;
		}
	}
	fclose(infile);//源文件读取完毕，开始清空内存
	for (int id = 0; id<BLOCKS; id++) {
		fseek(sortingfile, sizeof(Record)*id + blocklist[id].outcount, SEEK_SET);
		fwrite(&list[blocklist[id].instart], sizeof(Record), blocklist[id].incount, sortingfile);
		blocklist[id].outcount += blocklist[id].incount;
		blocklist[id].incount = 0;
		printf("%d %I64u %I64u \n", id, blocklist[id].key, blocklist[id].outcount);
	}
	free(list);
	fclose(sortingfile);
	printf("all blocks flushed! \n   ");
}

int merge(char tmppath[30], char outpath[30]) {
	FILE * tmpfile = fopen(tmppath, "ab");
	if (!tmpfile) {
		printf("fial write %s", tmppath);
	}
	FILE * outfile = fopen(outpath, "ab");
	if (!outfile) {
		printf("fial write %s", outpath);
	}

	for (int id = 0; id<(BLOCKS - 1); id++) {
		struct Record record = { 0,0 };
		struct Record *list = (Record*)calloc(INRECORDS, sizeof(Record));
		if (!list) {
			printf("fail to calloc list \n");
			return 1;
		}
		fread(&list, sizeof(Record), blocklist[id].outcount, tmpfile);
		qsort(list, blocklist[id].outcount, sizeof(Record), compare);
		fwrite(&list, sizeof(Record), blocklist[id].outcount, outfile);
		printf("block:%d %I64u records sorted \n", id, blocklist[id].outcount);
		free(list);
	}
	fclose(tmpfile);
	fclose(outfile);
	printf("%s sorted！\n", sorted);
}


int main() {

	//generate(num);  //产生待排序记录文件
	//display(unsort);  //查看
	sampling(unsort);  //抽样，描绘轮廓

	//singlesort(unsort, sorted);    //单机排序

	//validate(sorted); //验证
	//maketmp();   //产生临时大文件，两倍于源文件
	//assign(unsort,tmpsort);  //分发排序
	//merge(tmpsort,sorted);  //归并

	printf("\n time: %I64u  \n", clock() - start);

	gets(getline);  //暂停
	return 0;
}







