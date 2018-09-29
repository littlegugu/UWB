#include <stdio.h>
#include <pthread.h>
#include <winsock2.h>
#include <stdlib.h>
#include <unistd.h>	
#include <sched.h>
#include <string>
#include <time.h>
#include <math.h>
#include "cJSON.c"

#define HASH_MAX_SIZE 50000 /* 哈希节点最大值 */  
#define EVEN(x) ((x % 2) == 0)
#define TAG_SIZE 100 /* 手环数 */ 
#define X_MIN 0		/* 平面图x最小值 */ 
#define X_MAX 100	/* 平面图x最大值 */ 
#define Y_MIN 0		/* 平面图y最小值 */ 
#define Y_MAX 100	 /* 平面图y最大值 */ 
#define SMO_MAX_NUM 10	/* 准平稳条数 */ 
#define SP_LIM 5	// 速度	 
#define A 20 	//geoHash二分次数 
#define same_lim 3//字符串前n位相同不查 
#define LINE 30     /*txt文件行最大长度*/
#define ROOM_QUANT 3 /*房间区域数量*/
#define WALL_QUANT 4 /*房间内墙体数量*/
#define COR_X 0.69/*修正x*/
#define COR_Y 0.94/*修正y*/
#define GEO_BIN_LEN 8/*Geohash二进制字符串*/
#define GEO_STR_LEN 4/*Geohash字母字符串长度*/
#define BASE32_LAY_LEN 5 /*Geohash字符编码最大长度*/
#define BASE32_MIN_LEN 8 /*BASE32每层网络的字符长度，多少个二进编码为一个字符*/
#define GRID_QUANT 60000 /*栅格数量*/
#define STEP 30 /*步数*/

static const char base32_alphabet[32] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'j', 'k', 'm', 'n', 'p', 'q', 'r',
        's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

/* 可达区域 */ 
typedef struct Access{	
	char *key;
	int time=0;
	Access *next;
};

/* 哈希节点 */ 
typedef struct HashNode{
	char *key;
	/* 可达区域数组 这里是将1、3、5秒的都写在这里，也可以开三个数组 */
	Access *access;		/* 可达区域数组的首地址 */ 
	Access *accessRear;	/* 可达区域数组的最后一个 */ 
	HashNode *next;	/* 下一个节点 */ 
};

/*准平稳段*/
typedef struct smo{
	double x;//当前
	double y;
	long int t;
};

/*手环缓存区*/
typedef struct Tag{
	char *id;	/* 手环编号 */ 
	double cur_x;	//当前x 
	double cur_y;	// 当前y 
	long int cur_t;	//当前数据时间 
	char *cur_grid;	//当前区域编号 
	double pri_x;	//上一x 
	double pri_y;	//上一y 
	long int pri_t;	//上一次的时间 
	char *pri_grid;	//上一次的区域编号 
	int is_smo=0;	//是否平稳， 1为平稳，0为非平稳 
	smo smo_li[10];		//10条准平稳数据 
	int smo_num=0;	//已存放平稳数据,若为0则表示该手环没初始化过 
};

typedef struct wall{
    double top_x;
	double low_x;
	double top_y;
	double low_y;
};

typedef struct room{
    int room_num;/*房间区域序号*/
	int door_pos;/*门的位置*/
	int room_type;/*房间类型，0标准房间区域，1非标准房间区域，2不可通行房间区域*/
    double door_top_x;
    double door_low_x;
    double door_top_y;
    double door_low_y;
	double door_cer_x;
	double door_cer_y;
    double top_x;
    double low_x;
    double top_y;
    double low_y;
    struct wall walls[WALL_QUANT];
	int grid_num;/*房间内栅格数量*/
	int grid_list[300];
};

typedef struct area{
    double top_x;
    double low_x;
    double top_y;
    double low_y;
};

typedef struct grid{
    int numDec;
    char numStr[GEO_STR_LEN];
    int east;
    int south;
    int north;
    int west;
    double x;
    double y;
	int  areaNum;
	int areaCount;/*在房间区域内第几个栅格*/
    /*数组下标作为房间区域内的编号*/
};

// typedef struct areaGrid{
//     struct grid areaGridArr[GRID_QUANT];
//     int index = 0;//初始化后长度
// }areaGrid;

typedef struct Elem{
	char *elem;
};

/* 线程池 */ 
typedef struct threadpool_t{
	pthread_mutex_t lock;	/* 互斥锁 */ 
	pthread_cond_t queue_not_empty;		/* 任务队列不为空 */
	pthread_cond_t queue_not_full;	/* 任务队列不满 */

	pthread_t *threads;	/* 工作线程数组 */
	pthread_t rec_tid;	/* 接收线程 */

	SOCKET send_udp;	/* udp客户端 */ 
	sockaddr_in sin;
	int sin_len;

	Elem *queue;		/* 数据数组 */
	int queue_front;	/* 队头 */
	int queue_rear;		/* 队尾 */
	unsigned int queue_size;		/* 当前数据队列大小 */ 

};

/* 全局变量 */ 
int QUEUE_MAX_SIZE; 
int RECEIVE_PORT;	//接收端口 
int SEND_PORT; 	//发送端口 
char *SEND_ADDR;	//发送地址
int THREAD_NUM;	//工作线程数 
/* 全局变量 */ 
/* 手环数组 */ 
Tag tags[TAG_SIZE];
/* 当前拥有的哈希节点数目 */ 
int hash_table_size;
HashNode* hashTable[HASH_MAX_SIZE];

/* access_lock */
pthread_mutex_t access_lock;

/* 本地创建的客户端tcp_socket */
static SOCKET tcp_client;
/* 用于存储服务器的基本信息 */
static struct sockaddr_in tcp_server_in;
struct room rooms[ROOM_QUANT];/*房间区域数组*/
struct grid grids[GRID_QUANT];/*栅格数组*/
struct area areas;
int alpha;
double xSize,ySize;
int area_num[GRID_QUANT];


void reconnect()
{
	tcp_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(tcp_client == INVALID_SOCKET)
    {
        printf("invalid socket !");
        system("pause");
        return;
    }

    tcp_server_in.sin_family = AF_INET;    //IPV4协议族
    tcp_server_in.sin_port = htons(13107);  //服务器的端口号
    tcp_server_in.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //服务IP

    while(connect(tcp_client, (struct sockaddr *)&tcp_server_in, sizeof(tcp_server_in)) == SOCKET_ERROR)
    {
        printf("connection failed，reconnection after 3 seconds。\n");
        Sleep(3 * 1000);
    }
    printf("connect %s:%d\n", inet_ntoa(tcp_server_in.sin_addr), tcp_server_in.sin_port);
    send(tcp_client, "connect", strlen("connect"), 0);
}         

void Init_HashTable()
{
	hash_table_size = 0;
	memset(hashTable,0,sizeof(HashNode *)*HASH_MAX_SIZE);
	
	printf("init over! \n");
}

unsigned int hash(const char *str)
{
	const signed char *p = (const signed char*)str;
	unsigned int h = *p;
	if(h)
	{
		for(p+=1;*p!='\0';++p)
		{
			h = (h<<5)-h+*p;
		}
	}
	
	return h;
}

/* 哈希表插入 */ 
void hash_insert(const char* key,char *accessKey,int accessTime)
{
	if(hash_table_size == HASH_MAX_SIZE)
	{
		printf("out of memory! \n");
		return;
	}
	
	unsigned int pos = hash(key)%HASH_MAX_SIZE;
	
	HashNode *hashNode = hashTable[pos];
	
	int flag = 1;	/* 1为新，0为旧*/ 
	while(hashNode)
	{
		if(strcmp(hashNode->key,key)==0)
		{//已有节点 
			flag = 0;
			break;
		}
		hashNode = hashNode->next;
	}
	
	/* 创建新的节点 */ 
	if(flag)
	{
		printf("不存在键%s \n",key);
		HashNode *newNode = (HashNode *)malloc(sizeof(HashNode));
		memset(newNode,0,sizeof(HashNode));
		newNode->key = (char *)malloc(sizeof(char)*(strlen(key)+1));
		strcpy(newNode->key,key);
		
		Access *accessNode = (Access *)malloc(sizeof(Access));
		accessNode->key = (char *)malloc(sizeof(char)*(strlen(accessKey)+1));
		strcpy(accessNode->key,accessKey);
		accessNode->time = accessTime;
		accessNode->next = NULL;
		newNode->access = accessNode;
		newNode->accessRear = accessNode;
		
		newNode->next = hashTable[pos];
		hashTable[pos] = newNode;
		hash_table_size++;
	}
	
	/* 在原有节点上操作 */ 
	else
	{
		printf("已有键%s \n",key);
		Access *accessNode = (Access *)malloc(sizeof(Access));
		accessNode->key = (char *)malloc(sizeof(char)*(strlen(accessKey)+1));
		strcpy(accessNode->key,accessKey);
		accessNode->time = accessTime; 
		accessNode->next = NULL;
		
		/* 附加到原有的可达区域节点后面 */
		Access *p;
		p = hashNode->accessRear;
		p->next = accessNode;
		hashNode->accessRear = accessNode;
	}
	
	printf("The size of HashTable is %d now! \n",hash_table_size);
} 

/* 哈希表搜索 */ 
HashNode *hash_search(const char *key)
{
	unsigned int pos = hash(key)%HASH_MAX_SIZE;
	
	if(hashTable[pos])
	{
		HashNode *pHead = hashTable[pos];
		while(pHead)
		{
			if(strcmp(key,pHead->key)==0)
				return pHead;
			pHead = pHead->next;
			
		}
	}
	
	return NULL;
}


/* 打印整个哈希表 测试用 */ 
void display_hash_table()
{
	for(int i=0;i<HASH_MAX_SIZE;i++)
	{
		if(hashTable[i])
		{
			HashNode *phead = hashTable[i];
			while(phead)
			{
				printf("%s的可达区域是：\n",phead->key);
				
				Access *as = phead->access;
				while(as)
				{
					printf("区域编号：%s;时间:%d \n",as->key,as->time);
					as = as->next;
				}
				
				phead = phead->next;
			}
		}
	}	
} 

// char* base32_encode(char *bin_source)
// {
//     int i;
//     int j = 0;
//     static char str[20];

//     for(i=0;i<strlen(bin_source);++i){
//         if((i+1)%5==0){
//             j++;
//             int num = (bin_source[i]-'0')+(bin_source[i-1]-'0')*2\
//             +(bin_source[i-2]-'0')*2*2+(bin_source[i-3]-'0')*2*2*2\
//             +(bin_source[i-4]-'0')*2*2*2*2;

//             str[j-1] = base32_alphabet[num];
//         }

//     }
//     return str;
// }


// /* geohash编码 
// **	@param x double,坐标x
// **	@param y double,坐标y
// **  @param a int,精度 
// */
// char* GeoHash(double x, double y,int a)
// {
//     double x_mid,y_mid;
//     char x_ans[a];
//     char y_ans[a];
    
//     char ans[2*a+1];
//     double x_min = X_MIN,y_min = Y_MIN;
//     double x_max = X_MAX,y_max = Y_MAX;
    
//     int i,j;
    
//     for(i=0;i<a;i++){
//         y_mid = (y_min + y_max) / 2.0;
//         x_mid = (x_min + x_max) / 2.0;
        
//         /* 左边编码0，右边编码1 */ 
//         if(x - x_mid >= 0) 
//         {
//         	x_ans[i] = '1';
// 			x_min = x_mid;
// 		}	
//         else
// 		{
// 			x_ans[i] = '0';
// 			x_max = x_mid;
// 		} 
		   
//         if(y - y_mid >= 0) 
//         {
//         	y_ans[i] = '1';
// 			y_min = y_mid;
// 		}	
//         else
// 		{
// 			y_ans[i] = '0';
// 			y_max = y_mid;
// 		} 
        
//     }

//     //将两个坐标进行合并，组合为一个二进制编码
//     /* 奇位是y，偶位是x */
//     int k;
//     for(j=0,k=0;j<a;j++) 
// 	{
// 		ans[k] = x_ans[j];
// 		k +=2;
//     }
    
//     for(j=0,k=1;j<a;j++) 
// 	{
// 		ans[k] = y_ans[j];
// 		k +=2;
//     }

// 	ans[2*a] = '\0';
	
// 	printf("%s \n",ans);
	
//     return base32_encode(ans);
// }


/* 求速度 */ 
float  speed(smo s1,smo s2)
{
	float diff_time = (s2.t-s1.t)/1000.0;
	float dist = pow(pow(s1.x-s2.x,2.0)+pow(s1.y-s2.y,2.0),0.5);
	float sp = dist/diff_time;
	return sp;
}

/* 判断手环数据是否平稳 
** @param tag_pos int,手环在手环数组的下标 
*/
void is_steady(int tag_pos)
{
	printf("it's is_steady! \n");
	/* 如果判断已经平稳则修改tags[i].is_smo=1 */ 
	if(tags[tag_pos].smo_num<10)
	{//数据少于10，不做 
		printf("smo_num < 10! \n"); 
	}
	else
	{
		int i;
		int count=0;
		tags[tag_pos].is_smo=1;
		for(i=1;i<SMO_MAX_NUM;i++)
		{
			float sp = speed(tags[tag_pos].smo_li[i-1],tags[tag_pos].smo_li[i]);
			if(sp<SP_LIM)
			{
				count++;
				if(count==7)
				{	/* 多余7条平稳记录，即判定平稳 修改平稳字段*/ 
					tags[tag_pos].is_smo=1;
					printf("手环已稳定...\n");
					break;
				}
			}
		}
	}
	
}

/* 判断漂移算法 
** @param tag_pos int,手环在手环数组的下标 
** return confidentDegree int,该条记录的置信度 
*/
// float drift(int tag_pos)
// {
// 	int i1;
// 	int i2;
// 	int flag = 1;
// 	for(i1=0;i1<=same_lim;i1++)
// 	{
// 		if(tags[tag_pos].cur_grid[i1] != tags[tag_pos].pri_grid[i1])
// 		{
// 			flag = 0;
// 			printf("not nithboor area....\n");
// 			break;
// 		}
// 	}
	
// 	/* 前n位一样不用查表，代表邻近区间 */ 
// 	if(flag)
// 	{
// 		printf("neithboor area...\n");
// 		return 1.0;
// 	}
// 	else
// 	{
// 		HashNode *hns = hash_search(tags[tag_pos].pri_grid);
// 		if(hns!=NULL)
// 		{
// 			Access *p = hns->access; 
		
// 			printf("search access area...\n");
			
// 			while(p)
// 			{
// 				if(strcmp(p->key,tags[tag_pos].cur_grid)==0)
// 				{
// 					if(p->time >= tags[tag_pos].cur_t)
// 					{
// 						return 1.0;
// 					}
// 					else
// 					{
// 						float conf;
// 						conf= (tags[tag_pos].cur_t-p->time)/(5-p->time);
// 						return conf;
// 					}
// 				}
// 				p=p->next;
// 			}	
// 		}
// 		else
// 		{
// 			printf("错误，手环所在区域不在可达表中.... \n");
// 		}
		
// 	}
	
// 	return 0.0;
// }  

/* 解析传来的字符串 */ 
char *resolve_str(char *data)
{
	/* 用cJSON解析 */ 
	cJSON *json = cJSON_Parse(data);
	if(json)	//解析成功 
	{
		/* 判断是否已初始化该手环，0为无，1为有 */ 
		int flag = 0;
		/* 手环在手环数组里的下标 */ 
		int i;	
		/* 解析数据 */ 
		char *id = cJSON_GetObjectItem(json, "tagid")->valuestring;	/* id */ 
		double x = cJSON_GetObjectItem(json, "x")->valuedouble;	/* 坐标x */ 
		double y = cJSON_GetObjectItem(json, "y")->valuedouble;	/* 坐标y */ 
		char *time_str = cJSON_GetObjectItem(json, "time")->valuestring;	/* 时间 */ 
		long int time = atol(time_str);
		
		/* 在手环数组寻找手环 */ 
		for(i=0;i<TAG_SIZE;i++)
		{
			if(tags[i].smo_num == 0)
				break;
			
			if(strcmp(tags[i].id,id)==0)
			{
				//若存在则跳出 
				flag = 1;
				break;		
			} 
		}
		
		printf("该手环在数组的下标是：%d \n",i);
		
		/* 不存在的时候，进行初始化 */ 
		// if(!flag)
		// {
		// 	/* 为id开辟空间 */
		// 	printf("-------正在为手环%s初始化------- \n",id);
		// 	tags[i].id = (char *)malloc(strlen(id));
		// 	strcpy(tags[i].id,id);
			
		// 	tags[i].cur_x = tags[i].pri_x = x;
		// 	tags[i].cur_y = tags[i].pri_y = y;
		// 	tags[i].cur_t = tags[i].pri_t = time;
		// 	tags[i].smo_li[0].x = x;
		// 	tags[i].smo_li[0].y = y;
		// 	tags[i].smo_num = 1;
			 
		// 	/* 得到区域编码，使用geohash */
		// 	// char *geoHash = GeoHash(x,y,A);
		// 	tags[i].cur_grid = (char *)malloc(strlen(geoHash)+1);
		// 	tags[i].pri_grid = (char *)malloc(strlen(geoHash)+1);
		// 	strcpy(tags[i].cur_grid,geoHash);
		// 	strcpy(tags[i].pri_grid,geoHash);
			
		// 	printf("初始化%s ,所在区域是%s \n",tags[i].id,tags[i].cur_grid); 
		// }
		// else
		// {/* 如果手环已经在数组里面 */
		// 	printf("该手环已经存在！ \n");
		// 	/* 
		// 	** 先判断手环是不是已经平稳（即字段is_smo是否等于1） 
		// 	** 如果手环没有平稳，则进入判断是否平稳的函数
		// 	** 如果手环已经平稳，则进入判断是否漂移的函数 
		// 	*/ 
			
		// 	/* 修改数据 将上一条记录改成当前条，当前条更新为读取的数据*/
		// 	tags[i].pri_x = tags[i].cur_x;
		// 	tags[i].pri_y = tags[i].cur_y;
		// 	tags[i].pri_t = tags[i].cur_t;
		// 	strcpy(tags[i].pri_grid,tags[i].cur_grid);
			
		// 	tags[i].cur_x = x;	tags[i].cur_y = y;
		// 	tags[i].cur_t = time;	
		// 	strcpy(tags[i].cur_grid,GeoHash(x,y,A));
		// 	printf("手环%s当前位置为%s \n",tags[i].id,tags[i].cur_grid);
			
		// 	/* 如果手环不平稳 */
		// 	if(tags[i].is_smo==0)
		// 	{
		// 		/* 先处理更新数据 */ 
		// 		/* 如果里面准平稳段数据少于10 数据直接在最后面加*/
		// 		if(tags[i].smo_num<SMO_MAX_NUM)
		// 		{
		// 			tags[i].smo_li[tags[i].smo_num].x = x;
		// 			tags[i].smo_li[tags[i].smo_num].y = y;	
		// 			tags[i].smo_li[tags[i].smo_num].t = time;
		// 			tags[i].smo_num++;
		// 		}
		// 		else
		// 		{/*如果已经有10条了 清空第1条 将本次更新数据到最后一条*/ 
		// 			for(int j=1;j<SMO_MAX_NUM;j++)
		// 			{//前挪一条记录 
		// 				tags[i].smo_li[j-1].x = tags[i].smo_li[j].x;
		// 				tags[i].smo_li[j-1].y = tags[i].smo_li[j].y;
		// 				tags[i].smo_li[j-1].t = tags[i].smo_li[j].t;
		// 			}
		// 			tags[i].smo_li[SMO_MAX_NUM-1].x = x;
		// 			tags[i].smo_li[SMO_MAX_NUM-1].y = y;
		// 			tags[i].smo_li[SMO_MAX_NUM-1].t = time;
		// 		}
				
		// 		/* 进入判断是否平稳函数 只需要将手环下标传进去*/
		// 		is_steady(i);
		// 		printf("准平稳数据条数:%d \n",tags[i].smo_num);	
		// 	}
		// 	else
		// 	{/* 如果手环已经平稳进入到漂移算法 */ 
		// 		printf("Enter drift....\n");
		// 		float confidentDegree = drift(i);
		// 		cJSON_AddNumberToObject(json,"cd",confidentDegree);
		// 		char *jsonStr = cJSON_Print(json);
				
		// 		printf("置信度%.2f \n",confidentDegree);
		// 		return jsonStr;	
		// 	} 
		// }
		
	
	}
	
	return NULL;
} 

char *ReadData(FILE *fp,char *buf)
{
	return fgets(buf,LINE,fp);
}


// void *Read_Access_Area(void *arg)
// {
// 	/* 记录运行时间 */ 
// 	clock_t start,finish;
// 	double total_time;
	
// 	char *buf,*p;
// 	char *filename = (char *)arg;
	
// 	printf("filename: %s \n",filename);
	
// 	FILE *fp = fopen(filename, "r+");
// 	if(fp==NULL)
// 	{
// 		printf("open file fail！\n");
// 		return 0;
// 	}
	
// 	buf = (char *)malloc(LINE*sizeof(char));	
	
// 	start = clock();
// 	while(1)
// 	{
// 		pthread_mutex_lock(&access_lock);
		
// 		char *key1,*key2;
// 		char *time_str;
// 		int time;
// 		p = ReadData(fp,buf);//每次调用文件指针fp会自动后移一行
// 		if(!p)
// 		{
// 			break;	//每次调用文件指针fp会自动后移一行
// 		}
		
// 		key1 = (char *)malloc(sizeof(char)*9);
// 		key2 = (char *)malloc(sizeof(char)*9);
// 		time_str = (char *)malloc(sizeof(char)*2);
		
// 		strncpy(key1,p+1,8);
// 		strncpy(key2,p+13,8);
// 		strncpy(time_str,p+24,1);
// 		key1[8] = '\0';	key2[8] = '\0';	time_str[1] = '\0';
// 		time = atoi(time_str);
		
// 		hash_insert(key1,key2,time);
		
// 		free(key1);free(key2);free(time_str);
// 		pthread_mutex_unlock(&access_lock);	
// 	}
	
// 	finish = clock();
// 	total_time = (double)(finish-start)/CLOCKS_PER_SEC;
		
// 	printf("Runnig time of this process:%0.4f ms \n",total_time);

// }

/*分割txt字符串*/
double * split(char * data,double *list)
{
    char * re;
    re = strtok(data,",");
    int count = 0;
    while(re != NULL)
    {
        *(list + count++ ) = atof(re);
        // puts(re);
        re = strtok(NULL,",");
    }
	list[1]-=COR_X;
	list[2]-=COR_X;
	list[3]-=COR_Y;
	list[4]-=COR_Y;
    return list;
}

double max_double(double x,double y)
{
	return x>y?x:y;
}

double min_double(double x,double y)
{
    return x<y?x:y;
}

int Area_information_acquisition(void *arg){
	char *filename = (char *)arg;
	FILE * fp = NULL;/*文件指针*/
    char * buf;/*数据缓存区，记得free*/
	buf = (char *)malloc(LINE*sizeof(char));
	fp = fopen(filename,"r+");/*分割区域文件*/
	double list[4];
	if (fp) {
		printf("File %s reading...\n",filename);/* file exist. */
		while (fgets(buf,LINE,fp)!=NULL){
			split(buf,list);
			areas.top_x = list[1];
			areas.low_x = list[2];
			areas.top_y = list[3];
			areas.low_y = list[4];
		}
		printf("File %s read completed.\n",filename);
		fclose(fp);		
	}
	else {
		printf("File %s not exist!\n",filename);/* file not exist. */
		fclose(fp);
	}
	

}

/*二分法动态求Geohash精度*/
int dichotomy(double * range,double top,double low)
{
    double grid_size = (top-low)/2;
    int count = 1;
    while(*(range)>grid_size || grid_size>*(range+1))
    {
        // printf("%f\n",grid_size);
        // Sleep(1000);
        grid_size = grid_size/2;
        count++;
    }
    return count;
}

int Room_information_acquisition(void *arg)
{
	char *filename = (char *)arg;
	FILE * fp = NULL;/*文件指针*/
    char * buf;/*数据缓存区，记得free*/
	buf = (char *)malloc(LINE*sizeof(char));
	fp = fopen(filename,"r+");/*房间区域数据文件*/
	double buflist[5];
	double x_range[]={0,0};/*最大墙厚,最小门宽;0:下界；1:上界*/
    double y_range[]={0,0};
	double wx,wy,dx,dy;
	int count=0,rN = 0;/*count：数据到第几行；rN：房间号；*/
	if (fp) {
		printf("File %s reading...\n",filename);/* file exist. */
		while (fgets(buf,LINE,fp)!=NULL){
			// puts(buf);
			if (strlen(buf)<3){
				if (count=4){
					rooms[rN].room_type=0;/*标准区域*/
				}else{
					rooms[rN].room_type=1;/*非区域*/
				}
				count = 0;
				rN ++;
			}else{
				split(buf,buflist);
				if (count==0) {
					rooms[rN].room_num   = rN;/* 门 */
					rooms[rN].door_pos   = (int) buflist[0];
					rooms[rN].door_top_x = buflist[1];
					rooms[rN].door_low_x = buflist[2];
					rooms[rN].door_top_y = buflist[3];
					rooms[rN].door_low_y = buflist[4];
					rooms[rN].door_cer_x = (buflist[1]+buflist[2])/2;
					rooms[rN].door_cer_y = (buflist[3]+buflist[4])/2;
					dx = rooms[rN].door_top_x-rooms[rN].door_low_x;
					dy = rooms[rN].door_top_y-rooms[rN].door_low_y;
					
					if (x_range[1]==0 && y_range[1]==0) {
						if (dx>dy) {
							x_range[1] = dx;
						}else {
							y_range[1] = dy;
						}
					}else{
						if (dx>dy) {
							x_range[1] = min_double(x_range[1],dx);
						}else {
							y_range[1] = min_double(y_range[1],dy);
						}
					}
					
				}
				else {
					/* 墙 */
					rooms[rN].walls[count-1].top_x = buflist[1];
					rooms[rN].walls[count-1].low_x = buflist[2];
					rooms[rN].walls[count-1].top_y = buflist[3];
					rooms[rN].walls[count-1].low_y = buflist[4];
					wx = rooms[rN].walls[count-1].top_x-rooms[rN].walls[count-1].low_x;
					wy = rooms[rN].walls[count-1].top_y-rooms[rN].walls[count-1].low_y;
					if (wx<wy) {
						x_range[0] = max_double(x_range[0],wx);
					}
					else {
						y_range[0] = max_double(y_range[0],wy);
					}
					if (count==1) {
						rooms[rN].top_x = buflist[1];
						rooms[rN].low_x = buflist[2];
						rooms[rN].top_y = buflist[3];
						rooms[rN].low_y = buflist[4];
					}
					else {
						rooms[rN].top_x = max_double(rooms[rN].top_x,buflist[1]);
						rooms[rN].low_x = min_double(rooms[rN].low_x,buflist[2]);
						rooms[rN].top_y = max_double(rooms[rN].top_y,buflist[3]);
						rooms[rN].low_y = min_double(rooms[rN].low_y,buflist[4]);
					}
				}
				count++;
			}
		}
		printf("File %s read completed.\n",filename);
		fclose(fp);
	}
	else {
		printf("File %s not exist!\n",filename);/* file not exist. */
		fclose(fp);
	}
	free(buf);
	buf = NULL;
	printf("x:%f,%f\n",x_range[0],x_range[1]);
	printf("y:%f,%f\n",y_range[0],y_range[1]);
	//动态计算精度范围
    if(x_range[1]>y_range[1]){
        alpha      = dichotomy(x_range,areas.top_x,areas.low_x);//通过x范围计算二分次数
    }else{
        alpha      = dichotomy(y_range,areas.top_y,areas.low_y);//通过y范围计算二分次数
    }
	printf("%d\n",alpha);
	return 0;
}

/*补位*/
char* complement(char * str,int digit)
{
	int st = strlen(str);/*字符串长度*/
	if(st<digit)
	{
		/*不足digit位补位*/
		int diff = digit-st;
		char *tmp;
		tmp = (char *)malloc(digit*sizeof(char));
		strcpy(tmp,str);
		for(int i = 0; i < digit; i++)
		{
			if (i<diff)
				str[i] = '0';/* 补0 */
			else
				str[i] = tmp[i-diff];/* 移位 */
		}
		str[digit]='\0';
		free(tmp);
		tmp = NULL;
	}

	return str;
}

/*十进制转二进制*/
char* DectoBin(char* str, int count,int alpha)
{
    itoa(count, str, 2);/*二转十*/
    complement(str,alpha);/*不足alpha位，补位*/
    return str;
}

/*用Geohash二分法划分栅格，返回二进制字符串*/
char* Geohash_segGrid_Bin(char *str,int xCount,int yCount,int alpha)
{
    char xStr[GEO_BIN_LEN/2],yStr[GEO_BIN_LEN/2];
    DectoBin(xStr,xCount,alpha);
    DectoBin(yStr,yCount,alpha);
    // char str[GEOLEN];
    int j;
    for(int i = 0; i < alpha*2; i++)
    {
        str[i]=i/2;
        j = i/2;
        if (i%2==0) 
            str[i]=xStr[j];/* 偶x */
        else 
            str[i]=yStr[j];/* 奇 */
    }
    int end = alpha*2;
    str[end]='\0';
    return str;
}

int BintoDec(char * binStr)
{
	int decInt;	
	int sum   = 0;
	int j     = strlen(binStr)-1;
	for(int i = 0; i < strlen(binStr); i++)
	{
		decInt =  (int) binStr[i] - '0';
		sum    += decInt*(pow(2,j--));
	}
	return sum;
}

char* Base32_BintoStr(char *bin_source,char * code)
{
	char *tmpchar;
	int   num;
	int   count = 0;
	int   codeDig;
	// tmpchar     = (char *)malloc(BASE32_LIM);
    tmpchar   = (char *)malloc( BASE32_LAY_LEN *sizeof(char));
	complement(bin_source,BASE32_MIN_LEN);//不足8位补位
	for(int i = 0; i < strlen(bin_source) ;  i+=BASE32_LAY_LEN)
	{	
		strncpy(tmpchar, bin_source+i, BASE32_MIN_LEN);
		tmpchar[BASE32_LAY_LEN]= '\0';
		complement(tmpchar,BASE32_LAY_LEN);
		num           = BintoDec(tmpchar);
		code[count++] = base32_alphabet[num];
	}
	if (strlen(bin_source)%5 != 0)
		codeDig = strlen(bin_source)/5+1;
	else
		codeDig = strlen(bin_source)/5;
	code[codeDig]='\0';
    free(tmpchar);
	tmpchar = NULL;
	return code;
}

/*查找房间号码*/
int GetRoomNum(double x,double y)
{
    for (int rN = 0; rN < ROOM_QUANT; rN++)
    {
        if ((rooms[rN].low_x<=x+xSize && rooms[rN].top_x>x) && (rooms[rN].low_y<=y+ySize && rooms[rN].top_y>y))
            return rN;
    }
    return -1;
}

/*查看栅格通行情况*/
int inWall(double x,double y,int rN,int index)
{
    if(rN >= 0)
    {
        int cant[] = {0,0,0,0,0};
        for(int wN = 0; wN < WALL_QUANT; wN++)
        {
            // cant = {0,0,0,0,0};
            if ((rooms[rN].walls[wN].low_x<=x+ xSize && x<=rooms[rN].walls[wN].top_x) &&(rooms[rN].walls[wN].low_y<=y+ ySize && y<=rooms[rN].walls[wN].top_y))
            {
                /*栅格与墙有交集*/
                if(x+(xSize/2)-rooms[rN].walls[wN].top_x<0)
                    cant[0] = 1;/*0方向，不取等于*/
                if(y+(ySize/2)-rooms[rN].walls[wN].top_y<=0)
                    cant[1] = 1;/*1方向，取等于*/
                if(rooms[rN].walls[wN].low_x-(x+(xSize/2))<=0)
                    cant[2] = 1;/*2方向，取等于*/
                if(rooms[rN].walls[wN].low_y-(y+(ySize/2))<0)
                    cant[3] = 1;/*3方向，不取等于*/            
            }       
        }		
		grids[index].east  = cant[0];
        grids[index].north = cant[1];
        grids[index].west  = cant[2];
        grids[index].south = cant[3];
        return 0;
    }
    return 1;
}


/*使用geohash全图分割栅格*/
int * Geohash_Grid(void)
{
    xSize = (areas.top_x-areas.low_x)/pow(2,alpha);
    ySize = (areas.top_y-areas.low_y)/pow(2,alpha);
    double x,y;
    int xCount,yCount;
    char binStr[GEO_BIN_LEN];
    char geostr[GEO_STR_LEN];
    int count = 0;
    int index,rN;
    // int *area_num;
    int index_lim = (pow(2,alpha))*(pow(2,alpha));
    // area_num = (int *)malloc( index_lim *sizeof(int));
    // int *passDire;
    // passDire = (int *)malloc( LINE *sizeof(int));
    // FILE *fp = NULL;
    // fp = fopen("D:\\program\\UWB\\data\\gridec.txt","w");
    for( yCount = 0; yCount < pow(2,alpha); yCount++)
    {
        y = areas.low_y + ySize * yCount;
        for( xCount = 0; xCount < pow(2,alpha); xCount++)
        {
            x = areas.low_x + xSize * xCount;
			// printf("x:%f,x:%f\n",x,y);
            Geohash_segGrid_Bin(binStr,xCount,yCount,alpha);
            Base32_BintoStr(binStr,geostr);
            index = BintoDec(binStr);
            rN    = GetRoomNum(x,y);
			rooms[rN].grid_list[rooms[rN].grid_num]=index;
			rooms[rN].grid_num ++;
            // printf("%d\n",rN);
            area_num[index] = rN;
            if (rN>=0) {
				strcpy(grids[index].numStr,geostr);
				grids[index].areaNum   = rN;
				grids[index].x         = x;
				grids[index].y         = y;	
				grids[index].areaCount = rooms[rN].grid_num;
                inWall(x,y,rN,index);
            }
            // fprintf(fp,"x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,BintoDec(binStr));
            printf("x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,index);
        }
        
    }
    // fclose(fp);
    return area_num;
}

/* 初始化 比如可达区域 */ 
int Init_Read_txt()
{
	pthread_mutex_init (&access_lock,NULL);
	pthread_t pid1;
	pthread_t pid2;
	char file1[255];
	char file2[255];
	printf("Input the filename of all area information txt:");
	scanf("%s",&file2); 
	printf("Input the filename of room information txt:");
	scanf("%s",&file1);
	// pthread_create(&pid2,NULL,Area_information_acquisition,(void *)&file2);
	// pthread_create(&pid1,NULL,Room_information_acquisition,(void *)&file1);

	// pthread_create(&pid1,NULL,Read_Access_Area,(void *)&file1);
 
		
}



// int checkoom(int max , int grid num){
// 	if (num>=0 && num<=max) {
// 		return 0;
// 	}
// 	else {
// 		return 1;
// 	}
// }

/*赋值*/
int assignment(int row,int col,int value,int * mat,int all)
{
    if (col >=0 && col<all)
    {
        int index;
        index = row*all+col;
        mat[index] = value;
        if (row != col)
        {
            index = col*all+row;
            mat[index] = value;
        }
		printf("%d\n",index);
        return 0;
    }
    return -1;
}


/*  输入x,y坐标，输出二进制字符串
**	@param x doublex
**	@param y doubley
*/
int coorToBin(double x, double y, char *strBin)
{
    double x_mid,y_mid;
    double x_min = areas.low_x;
    double y_min = areas.low_y;
    double x_max = areas.top_x;
    double y_max = areas.top_y;
    if ((x>=x_min && x<=x_max) && (y>=y_min && y<=y_max))
    {
        puts("In area.");
        for(int i = 0; i < alpha; i++)
        {
            y_mid = (y_min + y_max) / 2.0;
            x_mid = (x_min + x_max) / 2.0;
            if (x<=x_mid) {
                strBin[i*2] = '0';/* 左0 */
                x_max = x_mid;
            }
            else {
                strBin[i*2] = '1';/* 右1 */
                x_min = x_mid;
            }
            if (y<=y_mid) {
                strBin[i*2+1] = '0';/* 左0 */
                y_max = y_mid;
            }
            else {
                strBin[i*2+1] = '1';/* 右1 */
                x_min = x_mid;
            }
        }
        strBin[alpha*2] = '\0';
        return 0;
    }else{
        puts("Out of area!");
        return 1;
    }     
}

int CoortoDec(int x , int y){
	str[GEO_BIN_LEN];
	if (coorToBin(x,y,str)==0) {
		return BintoDec(str);
	}else {
		return -1 ;
	}
}

int CheckArea(int dec){
	
	if (dec<0) {
		return -1;
	}
	else {
		return area_num[dec];
	}
	
}


int* connectedMatrix(int rN){
	int all  = rooms[rN].grid_num;
	int * mat;
	mat = (int *)malloc(all*all*sizeof(int));
	int i,j,k;
	int i_mat,i_grid,j_grid;
	for(i = 0; i < all*all; i++)
        mat[i] = 0;
	for(i_mat = 0; i_mat < all*all; i_mat++){
		assignment(i_mat,i_mat,1,mat,all );
		i_grid = rooms[rN].grid_list[i_mat];
		if (grids[i_grid].east==0) {
			j_grid = CoortoDec(grids[i_grid].x-xSize,grids[i_grid].y);
			if(CheckArea(j_grid)==rN){
				if ()
				{
					
				}
			}
		}
		else if(grids[i_grid].west==0) {
			/* code */
		}
		else {
			/* code */
		}
		

	}
}


int* connectedMatrix1(int rN)
{
    int all = rooms[rN].grid_num;
    // int mat[all*all];
    int *mat;
    mat = (int *)malloc(all*all*sizeof(int));
    int i,j;
    int p;
    for(i = 0; i < all*all; i++)
        mat[i] = 0;
    for(j = 0; j < all; j++)
    {
		i = rooms[rN].grid_list[j];
        assignment(i,i,1,mat,all);//对角边
		assignment(j,j,1,mat,all);
        if (grids[i].east == 0) {
            p = j + 1;/* 东 */
			// k = rooms[rN].grid_list[p];
            if(p >= 0 && p < all) {
				i = rooms[rN].grid_list[p];
				i = rooms[rN].grid_list[p];
				if (grids[i].west == 0){
					assignment(j,p,1,mat,all);
					if(grids[i].north == 0){
						p = j + pow(2,alpha);/* 北 */
						// k = rooms[rN].grid_list[p];
						if(p >= 0 && p < all) {
							i = rooms[rN].grid_list[p];
							if (grids[i].south == 0){
								assignment(i,p,1,mat,all);
								p = j + pow(2,alpha) + 1;/* 东北 */
								// k = rooms[rN].grid_list[p];
								assignment(i,p,1,mat,all);
								if (grids[i].west == 0) {
									p = j - 1;/* 西 */
									// k = rooms[rN].grid_list[p];
									if(p >= 0 && p < all) {
										i = rooms[rN].grid_list[p];
										if (grids[i].east == 0){
											assignment(i,p,1,mat,all);
											p = j + pow(2,alpha) - 1;/* 西北 */
											// k = rooms[rN].grid_list[p];
											assignment(i,p,1,mat,all);
										}
									}
								}
							}
						}
					}
                }
                if(grids[i].south == 0){
                    p = j - pow(2,alpha);/* 南 */
					// k = rooms[rN].grid_list[p];
                    if(p >= 0 && p < all) {
						i = rooms[rN].grid_list[p];
						if (grids[i].north == 0){
							assignment(i,p,1,mat,all);
							p = j - pow(2,alpha) + 1;/* 东南 */
							// k = rooms[rN].grid_list[p];
							assignment(i,p,1,mat,all);
							if (grids[i].west == 0) {
								p = j - 1;/* 西 */
								// k = rooms[rN].grid_list[p];
								if(p >= 0 && p < all) {
									i = rooms[rN].grid_list[p];
									if (grids[i].east == 0){
										assignment(i,p,1,mat,all);
										p = j + pow(2,alpha) - 1;/* 西北 */
										// k = rooms[rN].grid_list[p];
										assignment(i,p,1,mat,all);
									}
								}
							}  
						}                      
                    }                    
                }
            }
        }else if(grids[i].west == 0){
            p = j - 1;/* 西 */
			// k = rooms[rN].grid_list[p];
            if(p >= 0 && p < all) {
				i = rooms[rN].grid_list[p];
				if (grids[i].east == 0){
						assignment(i,p,1,mat,all);
						if(grids[i].north == 0){
							p = j + pow(2,alpha);/* 北 */
							// k = rooms[rN].grid_list[p];
							if(p >= 0 && p < all) {
								i = rooms[rN].grid_list[p];
								if (grids[i].south == 0){
									assignment(i,p,1,mat,all);
									p = j + pow(2,alpha) - 1;/* 西北 */
									// k = rooms[rN].grid_list[p];
									assignment(i,p,1,mat,all);
								}
							}
						}
					}
                }
                if(grids[i].south == 0){
                    p = j - pow(2,alpha);/* 南 */
					// k = rooms[rN].grid_list[p];
                    if(p >= 0 && p < all) {
						i = rooms[rN].grid_list[p];
						if (grids[i].north == 0){
							assignment(i,p,1,mat,all);
							p = j - pow(2,alpha) - 1;/* 西南 */
							// k = rooms[rN].grid_list[p];
							assignment(i,p,1,mat,all);
						}
                    }
                }                                
            
        }else{
            if(grids[i].north == 0){
                p = j + pow(2,alpha);/* 北 */
				// k = rooms[rN].grid_list[p];
                if(p >= 0 && p < all) {
					i = rooms[rN].grid_list[p];
					if (grids[i].south == 0)
                    	assignment(i,p,1,mat,all);
				}
            }
            if(grids[i].south == 0){
                p = j - pow(2,alpha);/* 南 */
				// k = rooms[rN].grid_list[p];
                if(p >= 0 && p < all) {
					i = rooms[rN].grid_list[p];
					if (grids[i].north == 0)
                    	assignment(i,p,1,mat,all);
				}
            }
        }
	}

    // int k;
    // for(i = 0; i < all; i++)
    // {
    //     for(int j = 0; j < all; j++)
    //     {
    //         k = i*all + j;
    //         printf("%d,",mat[k]);
    //     }
    //     printf("\n");
    // }
    
    return mat;
}


int * dot(int * a_mat, int * b_mat,int a_row,int b_row,int a_col,int b_col)
{
    if(a_col != b_row)
    {
        puts("Error:Two matrix can not be be multiplied!");
        return NULL;
    }else{
        // double * ap = (double *)a;//获取矩阵
        // double * bp = (double *)b;//获取矩阵a的首地址 
        int value;
        int i,j,k;
        int *c_mat;
        c_mat = (int *)malloc(a_row * b_col*sizeof(int));
        for( i = 0; i < a_row; i++)
        {
            for( j = 0; j < b_col; j++)
            {
                value = 0;
                for( k = 0; k < b_row; k++)
                    value += ((*(a_mat+(j*a_row)+k))*(*(b_mat+(k*a_row)+i)));
                *(c_mat+i+(j*a_row))=value;
            }
        }
        return c_mat;
    }
}

// int test(void){
// 	int *matA = connectedMatrix(0);
// 	printf("%d\n",matA[590]);
// 	return 0;
// }

int canGet(void)
{
    int rN,row,i,j,count;
    for(rN = 0; rN < ROOM_QUANT; rN++)
    {
        int *matA,*tmp,*matB;
        int accesstime;
        matA = connectedMatrix(rN);
		printf("room %d\n",rN);
		for (int i = 0; i < pow(rooms[rN].grid_num,2); i++)
		{
			printf("%d,",matA[i]);
			if ((i+1)%rooms[rN].grid_num==0)
			{
				printf("\n");
			}
		}
        // matB = matA;
        // row = rooms[rN].grid_num;
        // char *key1,*key2;
        // int flag;
        // for(int time = 1; time < STEP; time++)
        // {
        //     tmp  = dot(matA,matB,row,row,row,row);
        //     accesstime = ((int)(time/4))+1;
        //     count = 0;
        //     for(i = 0; i < row; i++)
        //     {
        //         key1 = grids[rN].numStr;
                
        //         for(j = 0; j < row; j++)
        //         {
        //             if (tmp[i*row+j]>0)
        //             {
        //                 key2 = grids[rN].numStr;
        //                 hash_insert(key1,key2,accesstime);
        //             }
        //         }
        //     }
        //     matB = tmp;
        // }
    }
    return 0;
}

/* 工作函数  */
void *func_process(void *thr_pool)
{
	threadpool_t *pool = (threadpool_t *)thr_pool;
	
	/* 初始化udp客户端 */
	WORD socketVersion = MAKEWORD(2,2);
    WSADATA wsaData; 
    if(WSAStartup(socketVersion, &wsaData) != 0)
    {
        return 0;
    }
    SOCKET udpclient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    pool->send_udp = udpclient;
    
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SEND_PORT);
    sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");	/* SEND_ADDR */
    int len = sizeof(sin);	 

	/* 记录运行时间 */ 
	clock_t start,finish;
	double total_time;
	
    
	while(1)
	{	
		/* 加锁 */
		pthread_mutex_lock (&(pool->lock));
		
		start = clock();
		
		while(pool->queue_size==0){
			printf("thread-0x%x wating!\n",(unsigned int)pthread_self());
			pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));
		} 
		
		char *a_data = pool->queue[pool->queue_front].elem;
		pool->queue[pool->queue_front].elem = NULL;
		pool->queue_front = (pool->queue_front+1)%QUEUE_MAX_SIZE;
		pool->queue_size--;

		
		/* 通知可以添加新任务 */
		pthread_cond_broadcast(&(pool->queue_not_full));
		
		/* 处理刚取出的数据 */ 
		  
		char *result = resolve_str(a_data);
		
		if(result!=NULL)
			sendto(udpclient,result, strlen(result), 0, (sockaddr *)&sin, len);	//发送udp包 
		
		finish = clock();
		total_time = (double)(finish-start)/CLOCKS_PER_SEC;
		
		printf("Runnig time of this process:%0.4f ms \n",total_time);
		
		/* 解锁 */ 
		pthread_mutex_unlock(&(pool->lock));	
	}
	
}

//接收函数 
void *func_receive(void *thr_pool)
{
	threadpool_t *pool = (threadpool_t *)thr_pool;
	
	while(1)
	{	
		pthread_mutex_lock(&(pool->lock));
		
		/* 如果队列满了，调用wait阻塞 */  
		while(pool->queue_size == QUEUE_MAX_SIZE)
		{
			printf("queue full!wating! \n"); 
			pthread_cond_wait(&(pool->queue_not_full),&(pool->lock));
		}
		
		char recvData[255];  	/* 声音接收数据数组 最大255*/  
		int ret = recv(tcp_client,recvData,255,0);
		if (ret > 0)
        {
          	recvData[ret] = '\0';
            pool->queue[pool->queue_rear].elem = recvData;
            pool->queue_rear = (pool->queue_rear+1)%QUEUE_MAX_SIZE;	/* 环状队列 */ 
            pool->queue_size++;
            printf("%s \n",recvData);
            printf("Receive data,queue_size is %d now!\n",pool->queue_size);
            
            /* 添加完数据后，队列不为空，唤醒一个工作线程 */
			pthread_cond_signal(&(pool->queue_not_empty)); 
    	}
		
		else if(ret == 0)
        {
            //当ret == 0 说明服务器掉线。
            printf("Lost connection , Ip = %s\n", inet_ntoa(tcp_server_in.sin_addr));
            closesocket(tcp_client);
            reconnect();//重连
        }
        else
        {
        	//当ret < 0 说明出现了异常 例如阻塞状态解除，或者读取数据时出现指针错误等。
            //所以我们这里要主动断开和客户端的链接。
            printf("Something wrong of %s\n", inet_ntoa(tcp_server_in.sin_addr));
            closesocket(tcp_client);
            reconnect();//重连
        }	
	
		pthread_mutex_unlock(&(pool->lock));	
	}
	
}


/* 初始化 */ 
threadpool_t *init(int thr_num)
{
	int i;
	threadpool_t *pool = NULL;
	
	/* 初始化tcp客户端 */ 
	WORD socket_version;
	WSADATA wsadata; 
	socket_version = MAKEWORD(2,2);

	while(WSAStartup(socket_version, &wsadata) != 0)
    {
        printf("wsastartup error!\n");
        reconnect();
    }
	
	
	do
	{
		/* 初始化线程池 */
		if((pool=(threadpool_t *)malloc(sizeof(threadpool_t)))==NULL)
		{
			printf("malloc threadpool fail! \n");
			break;
		}
		
		/* 初始化互斥锁和条件变量 */
		if ( pthread_mutex_init(&(pool->lock), NULL) != 0 ||
       		pthread_cond_init(&(pool->queue_not_empty), NULL) !=0  ||
       		pthread_cond_init(&(pool->queue_not_full), NULL) !=0)
      	{
        	printf("init lock or cond false;\n");
        	break;
      	}
		  
		/* 队列开空间 */	
      	pool->queue = (Elem *)malloc(sizeof(Elem)*QUEUE_MAX_SIZE);
      	if(pool->queue == NULL)
      	{
      		printf("malloc queue fail! \n");
      		break;
		}
      	/* 队列初始化 */
		pool->queue_front = 0;
		pool->queue_rear = 0;   
      	pool->queue_size = 0;
      	
      	/* 给工作线程开空间 */
		pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*thr_num);
		if(pool->threads == NULL)
		{
			printf("malloc threads fail! \n");
			break;
		} 
		memset(pool->threads,0,sizeof(pthread_t)*thr_num);
		/* 启动工作线程 */
		for(i=0;i<thr_num;i++)
		{
			pthread_create(&(pool->threads[i]),NULL,func_process,(void *)pool);
			printf("start thread 0x%x....\n",(unsigned int)pool->threads[i]);	
		} 
		
		/* 启动接收线程 */
		pthread_create(&(pool->rec_tid),NULL,func_receive,(void *)pool);
		
		return pool; 
	}while(0);
	
	return NULL;
}

int readConfiguration(char *fileAddr)
{
	printf("------正在读取配置文件%s--------\n",fileAddr);
	
	/* 声明cJSON对象 */ 
	cJSON *json = NULL;	 
	
	/* 以读形式打开配置json文件 */
	FILE *fp = fopen(fileAddr,"rb");
	if(fp == NULL)
	{
		printf("Open file fail! \n");
		return 0;
	}
	
	fseek(fp,0,SEEK_END);
	int len = ftell(fp);
	fseek(fp,0,SEEK_SET);
	
	char *jsonStr = (char *)malloc(sizeof(char)*(len+1)); /* 申请空间 */ 
	fread(jsonStr,1,len,fp);	/* 读取配置文件为jsonStr字符串 */ 
	fclose(fp);	/* 关闭文件 */ 
	
	json = cJSON_Parse(jsonStr);	/* 转换为json对象 */ 
	if(json == NULL)
	{
		printf("Read configuration fail! \n");
		return 0;
	}
	
	/* 读取最大数据数量 */ 
	cJSON *sub = cJSON_GetObjectItem(json,"QUEUE_MAX_SIZE");	/* 最大数据 */
	if(sub == NULL)
	{
		printf("Read QUEUE_MAX_SIZE fail! \n");
		return 0;	
	}
	QUEUE_MAX_SIZE = sub->valueint;
	printf("QUEUE_MAX_SIZE:%d \n",QUEUE_MAX_SIZE);
	
	/* 读取接收端口 */ 
	sub = cJSON_GetObjectItem(json,"RECEIVE_PORT");	/* 接收端口 */
	if(sub == NULL)
	{
		printf("Read RECEIVE_PORT fail! \n");
		return 0;	
	}
	RECEIVE_PORT = sub->valueint;
	printf("RECEIVE_PORT:%d \n",RECEIVE_PORT);
	
	/* 读取发送端口 */ 
	sub = cJSON_GetObjectItem(json,"SEND_PORT");	/* 发送端口 */
	if(sub == NULL)
	{
		printf("Read SEND_PORT fail! \n");
		return 0;	
	}
	SEND_PORT = sub->valueint;
	printf("SEND_PORT:%d \n",SEND_PORT);
	
	/* 读取发送地址 */
	sub = cJSON_GetObjectItem(json,"SEND_ADDR");	/* 发送地址 */
	if(sub == NULL)
	{
		printf("Read SEND_ADDR fail! \n");
		return 0;	
	}
	SEND_ADDR = sub->valuestring;
	printf("SEND_ADDR:%s \n",SEND_ADDR); 
	
	/* 读取工作线程数 */ 
	sub = cJSON_GetObjectItem(json,"THREAD_NUM");	/* 工作线程数 */
	if(sub == NULL)
	{
		printf("Read THREAD_NUM fail! \n");
		return 0;	
	}
	THREAD_NUM = sub->valueint;
	printf("THREAD_NUM:%d \n",THREAD_NUM);
	
	cJSON_Delete(json);
	
	return 1;
}


// int main()
// {
// 	if(!readConfiguration("config.json"))
// 		return 0;
	
// 	Init_HashTable();
// 	Init_Read_txt();

// 	display_hash_table();
	
// 	/* 初始化线程池，开启THREAD_NUM条工作线程 */ 
// 	threadpool_t *pool = init(THREAD_NUM);	
// 	/* 直到接收线程停止，程序终止 */ 
// 	pthread_join(pool->rec_tid,NULL);
	
// }
main(int argc, char const *argv[])
{
	char filename1[] = "area.txt";
	char filename2[] = "room.txt";
	Area_information_acquisition((void *)&filename1);
	Room_information_acquisition((void *)&filename2);
	Geohash_Grid();
	Init_HashTable();
	// canGet();
	// test();
	return 0;
}


