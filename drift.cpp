#include <stdio.h>
#include <pthread.h>
#include <winsock2.h>
#include <stdlib.h>
#include <unistd.h>	
#include <sched.h>
#include <string.h>
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
#define LINE 30
#define SMO_SIZE 10
// #define LINE 100
#define COR_X 0.69
#define COR_Y 0.94
#define GEOLEN 10
#define BASE32_LEN 5
#define BASE32_LIM 8
#define BASE32_CODE 10
#define LENF 8
#define ROOMNUM 2
#define WALLNUM 6
#define GRIDQUANTITY 20000
#define ROOMQUANTITY 2
#define STEP 30
// #define HASH_MAX_SIZE 10000
#define TIME_LIM 10

static const char base32_alphabet[32] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'j', 'k', 'm', 'n', 'p', 'q', 'r',
        's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

/* 可达区域 */ 
typedef struct Access{	
	char *key;
	int time;
	struct Access *next;
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
	long long t;
    double dt;
};

/*手环缓存区*/
typedef struct Tag{
	char *id;	/* 手环编号 */ 
	double cur_x;	//当前x 
	double cur_y;	// 当前y 
	long long cur_t;	//当前数据时间 
	char *cur_grid;	//当前区域编号 
	double pri_x;	//上一x 
	double pri_y;	//上一y 
	long long pri_t;	//上一次的时间 
	char *pri_grid;	//上一次的区域编号 
	int is_smo=0;	//是否平稳， 1为平稳，0为非平稳 
	smo smo_li[SMO_SIZE];		//10条准平稳数据 
	int smo_num=0;	//已存放平稳数据,若为0则表示该手环没初始化过
	int cur_decGrid; 
    int pri_decGrid;
};

typedef struct Elem{
	char *elem;
};

typedef struct wall{
    double top_x;
	double low_x;
	double top_y;
	double low_y;
    int wall_num;
};

// struct wall;
typedef struct room{
    int room_num;
	int door_num;
    double door_top_x;
    double door_low_x;
    double door_top_y;
    double door_low_y;
    double top_x;
    double low_x;
    double top_y;
    double low_y;
    struct wall walls[WALLNUM];
};

typedef struct area{
    double top_x;
    double low_x;
    double top_y;
    double low_y;
};

typedef struct grid{
    int numDec;
    char numStr[GEOLEN];
    // int pos[4];
    int east;
    int south;
    int north;
    int west;
    double x;
    double y;
    /*数组下标作为房间区域内的编号*/
}grid;

typedef struct areaGrid{
    struct grid areaGridArr[GRIDQUANTITY];
    int index = 0;//初始化后长度
};

struct room rooms[ROOMNUM];//声明两个房间
struct area areas;
struct areaGrid grids[ROOMQUANTITY];
// double area[4];
double door_x = 0.0;
double door_y = 0.0;
double wall_x = 0.0;
double wall_y = 0.0;
int alpha=0;//二分次数
/* 当前拥有的哈希节点数目 */ 
int now_tag = 0;

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
int * areaGridDec = NULL;
/* access_lock */
pthread_mutex_t access_lock;

/* 本地创建的客户端tcp_socket */
static SOCKET tcp_client;
/* 用于存储服务器的基本信息 */
static struct sockaddr_in tcp_server_in;


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
		printf("Not exist:%s ! \n",key);
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
		
		newNode->next = hashTable[pos];
		hashTable[pos] = newNode;
		hash_table_size++;
	}
	
	/* 在原有节点上操作 */ 
	else
	{
		printf("Already has key:%s ! \n",key);
		Access *as = hash_search(key)->access;
		int asflag = 1;
		while (as)
		{
			if (strcmp(as->key,accessKey) != 0)
			{
				asflag = 0;
				break;
			}
			as = as->next;
		}
		if(asflag==0)
		{
			Access *accessNode = (Access *)malloc(sizeof(Access));
			accessNode->key = (char *)malloc(sizeof(char)*(strlen(accessKey)+1));
			strcpy(accessNode->key,accessKey);
			accessNode->time = accessTime; 
			accessNode->next = NULL;
			
			/* 附加到原有的可达区域节点后面 */
			Access *p;
			p = hashNode->access;
			while(p->next)
			{
				p = p->next;	
			}
			p->next = accessNode;
		}
	}
	
	printf("The size of HashTable is %d now! \n",hash_table_size);
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

int binToDec(char * binStr)
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

char* complement(char * str,int digit)
{
	int st = strlen(str);/*字符串长度*/
	if(st<digit)
	{
		/*不足digit位补位*/
		int diff = digit-st;
		char tmp[LENF];
		strcpy(tmp,str);
		for(int i = 0; i < digit; i++)
		{
			if (i<diff)
				str[i] = '0';/* 补0 */
			else
				str[i] = tmp[i-diff];/* 移位 */
		}
		str[digit]='\0';
	}
	return str;
}

char* base32_encode(char *bin_source,char * code)
{
	char *tmpchar;
	int   num;
	int   count = 0;
	int   codeDig;
	// tmpchar     = (char *)malloc(BASE32_LIM);
    tmpchar   = (char *)malloc( LENF *sizeof(char));
	complement(bin_source,BASE32_LIM);//不足8位补位
	for(int i = 0; i < strlen(bin_source) ;  i+=BASE32_LEN)
	{	
		strncpy(tmpchar, bin_source+i, BASE32_LEN);
		tmpchar[BASE32_LEN]= '\0';
		complement(tmpchar,BASE32_LEN);
		num           = binToDec(tmpchar);
		code[count++] = base32_alphabet[num];
	}
	if (strlen(bin_source)%5 != 0)
		codeDig = strlen(bin_source)/5+1;
	else
		codeDig = strlen(bin_source)/5;
	code[codeDig]='\0';
    free(tmpchar);
	return code;
}

/*
* 分割字符串
*/
void split(char * data,double *list)
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
    // return list;
}

double max_double(double x,double y)
{
	return x>y?x:y;
}

double min_double(double x,double y)
{
    return x<y?x:y;
}

int init_area(void)
{
    int flags = 0;
    int count = 0;
    double * list;
    list  = (double *)malloc( LINE *sizeof(double));
    int num;
    double top_x  = -10000.0;
    double low_x  = 10000.0;
    double top_y  = -10000.0;
    double low_y  = 10000.0;
    double x;
    double y;
    int count_x = 0;
    int count_y = 0;
	FILE * fp = NULL;/*文件指针*/
    char * buf;/*数据缓存区，记得free*/
	buf = (char *)malloc(LINE*sizeof(char));
    fp = fopen("D:\\program\\UWB\\data\\room.txt","r+");/*房间区域数据文件*/
	if (fp!=NULL) {
		puts("room.txt file reading...");
		while (fgets(buf,LINE,fp)!=NULL)
		{
			// puts(buf);
            if (strlen(buf)<=3)
            {
                rooms[flags].top_x = top_x;
                rooms[flags].low_x = low_x;
                rooms[flags].top_y = top_y;
                rooms[flags].low_y = low_y;
                flags++;
                count = 0;
                top_x = -10000.0;
                low_x = 10000.0;
                top_y = -10000.0;
                low_y = 10000.0;

            }
            else
            {
                split(buf,list);
                if (count== 0) {
                    rooms[flags].room_num = flags;
                    rooms[flags].door_num = (int) list[0];
                    rooms[flags].door_top_x = list[1]-COR_X;
                    rooms[flags].door_low_x = list[2]-COR_X;
                    rooms[flags].door_top_y = list[3]-COR_Y;
                    rooms[flags].door_low_y = list[4]-COR_Y;
                    door_x += list[1]-list[2];
                    door_y += list[3]-list[4];

                }
                else {
                    num = list[0];
                    rooms[flags].walls[num].top_x=list[1]-COR_X;
                    rooms[flags].walls[num].low_x=list[2]-COR_X;
                    rooms[flags].walls[num].top_y=list[3]-COR_Y;
                    rooms[flags].walls[num].low_y=list[4]-COR_Y;
                    rooms[flags].walls[num].wall_num = num;
                    top_x = max_double(top_x,list[1]-COR_X);
                    low_x = min_double(low_x,list[2]-COR_X);
                    top_y = max_double(top_y,list[3]-COR_Y);
                    low_y = min_double(low_y,list[4]-COR_Y);
                    x = list[1]-list[2];
                    y = list[3]-list[4];
                    // printf("x:y:%f,%f\n",x,y);
                    wall_x += x<y ? x:0;
                    wall_y += x>y ? y:0;
                    count_x +=x<y ? 1:0;
                    count_y +=x>y ? 1:0;
                    
                }
                count++;
            }            
		}
		fclose(fp);
		puts("room file close!");
	}else{
        /*文件读取错误！*/
		puts("room file error!");
		fclose(fp);
	} 
    
    door_x = door_x/flags;
    door_y = door_y/flags;
    wall_x = wall_x/count_x;
    wall_y = wall_y/count_y;   
    fp = fopen("D:\\program\\UWB\\data\\area.txt","r+");
    
    if (fp!=NULL) {
        fgets(buf,LINE,fp);
        puts("area.txt file reading...");
        split(buf,list);
        areas.top_x=list[0]-COR_X;
        areas.low_x=list[1]-COR_X;
        areas.top_y=list[2]-COR_Y;
        areas.low_y=list[3]-COR_Y;
        fclose(fp);
        puts("area file close!");
    }
    else {
        /*文件读取错误！*/
		puts("area file error!");
		fclose(fp);
    }
    fclose(fp);
    free(list);
    free(buf);
    return 0;
}


/*  将x,y转换为二进制字符串 
**	@param x double,坐标x
**	@param y double,坐标y
*/
char * coorToBin(double x, double y)
{
	// a = alpha;
    double x_mid,y_mid;
    char x_ans[alpha];
    char y_ans[alpha];
    
    char ans[2*alpha+1];
    double x_min = areas.low_x;
	double y_min = areas.low_y;
	double x_max = areas.top_x;
	double y_max = areas.top_y;
    
    int i,j;
    
    for(i=0;i<alpha;i++){
        y_mid = (y_min + y_max) / 2.0;
        x_mid = (x_min + x_max) / 2.0;
        
        /* 左边编码0，右边编码1 */ 
        if(x - x_mid >= 0) 
        {
        	x_ans[i] = '1';
			x_min = x_mid;
		}	
        else
		{
			x_ans[i] = '0';
			x_max = x_mid;
		} 
		   
        if(y - y_mid >= 0) 
        {
        	y_ans[i] = '1';
			y_min = y_mid;
		}	
        else
		{
			y_ans[i] = '0';
			y_max = y_mid;
		} 
        
    }

    //将两个坐标进行合并，组合为一个二进制编码
    /* 奇位是y，偶位是x */
    int k;
    for(j=0,k=0;j<alpha;j++) 
	{
		ans[k] = x_ans[j];
		k +=2;
    }
    
    for(j=0,k=1;j<alpha;j++) 
	{
		ans[k] = y_ans[j];
		k +=2;
    }

	ans[2*alpha] = '\0';
	
	printf("%s \n",ans);
	// char code[BASE32_CODE];
	// base32_encode(ans,code);
    return ans;
}



double speed(double dist,double dt){
	return dist/dt;
}

double dists(smo s1,smo s2){
	return (double) (pow(pow(s1.x-s2.x,2.0)+pow(s1.y-s2.y,2.0),0.5));
}

/* 判断手环数据是否平稳 
** @param tag_pos int,手环在手环数组的下标 
*/
void is_steady(int tag_pos)
{
	int i;
	int count=0;
	double sp;
	char * binstr;
	for(i=1;i<SMO_MAX_NUM;i++)
	{
		double sp = speed(dists(tags[tag_pos].smo_li[i-1],tags[tag_pos].smo_li[i]),
		tags[tag_pos].smo_li[i-1].dt);
		if(sp<SP_LIM)
		{
			count++;
			if(count==7)
			{	
				/* 多于7条平稳记录，跳*/ 
				tags[tag_pos].is_smo=1;
				//查栅格
				binstr = coorToBin(tags[tag_pos].cur_x,tags[tag_pos].cur_y);
                tags[tag_pos].cur_decGrid = binToDec(binstr);
                base32_encode(binstr,tags[tag_pos].cur_grid);
				break;
			}
		}
	}
}

/* 判断漂移算法 
** @param tag_pos int,手环在手环数组的下标 
** return confidentDegree int,该条记录的置信度 
*/
float drift(int tag_pos,int dt){
    int area_cur = areaGridDec[tags[tag_pos].cur_decGrid];
    int area_pri = areaGridDec[tags[tag_pos].cur_decGrid];
    char * key   = tags[tag_pos].pri_grid;
    char * askey = tags[tag_pos].cur_grid;
    if(area_cur < 0 || area_pri < 0 ){
        //其中一个在不可达区域
        return 0.0;
    }else if(area_cur == area_pri){
        Access *as = hash_search(key)->access;
		while (as)
		{
			if (strcmp(as->key,askey) == 0)
			{
                if (as->time >= dt)
                    return 1;
                else 
                    return 0.2;//可达时间不同
                
			}
			as = as->next;
		}
    }else{
        /*不同区域*/
        return 0;
    }

}
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

long long timestamp(char * time_str){
    int i,j,count;
    int num[7];
    struct tm stm; 
    char decos[]   = "- :.";
    char deco[]    = "a";
    char *re       = NULL;
    char *str      = time_str;
    int list[]     = {2,1,2,1};
    int time_count = 0;
    int all_count  = 0;
    memset(&stm,0,sizeof(stm)); 
    for (i = 0; i < strlen(decos); i++)
    {
        count = 0;
        deco[0] = decos[i];
        re = strtok(str,deco);
        while(re != NULL){
            if((count<list[i]) or ((i==(strlen(decos)-1))and count==list[i]))
            {
                num[all_count++] = atoi(re);
            }else{
                str = re;
            }
            re = strtok(NULL,deco);
            count++;
        }
    }
    stm.tm_year  = num[0]-1900;  
    stm.tm_mon   = num[1]-1;  
    stm.tm_mday  = num[2];  
    stm.tm_hour  = num[3];  
    stm.tm_min   = num[4];  
    stm.tm_sec   = num[5];
    long long t1 = (long long)mktime(&stm)*1000+num[6];
    return t1;
}

/* 解析传来的字符串 */ 
char * resolve_str(char *data)
{

	/* 用cJSON解析 */ 
	cJSON *json = cJSON_Parse(data);
	if(json)	//解析成功 
	{
		int flag = 0;//判断是否已初始化该手环，0为无，1为有
		int i,nn;//i:手环下标；nn：当前smo_li号码	
        double dt;//时间差
		double confidentDegree;//置信度
		/* 解析数据 */ 
		char *id = cJSON_GetObjectItem(json, "tagid")->valuestring;	/* id */ 
		double x = cJSON_GetObjectItem(json, "xcoord")->valuedouble;	/* 坐标x */ 
		double y = cJSON_GetObjectItem(json, "ycoord")->valuedouble;	/* 坐标y */ 
		char *time_str = cJSON_GetObjectItem(json, "time")->valuestring;	/* 时间 */ 
		long long time = timestamp(time_str);
		/* 在手环数组寻找手环 */ 
		for(i=0;i<TAG_SIZE;i++)
		{
			if(strcmp(tags[i].id,id)==0)
			{
				//若存在则跳出 
				flag = 1;
                printf("该手环在数组的下标是：%d \n",i);
				break;		
			} 
		}
		if(!flag)
		{
			/* 
			**手环编号不存在,为手环初始化
			 */
			printf("-------正在为手环%s初始化------- \n",id);
			tags[now_tag].id = (char *)malloc(strlen(id));
			strcpy(tags[now_tag].id,id);//为id开辟空间
			tags[now_tag].cur_x = x;
            tags[now_tag].cur_y = y;
            tags[now_tag].cur_t = time;
			tags[now_tag].smo_li[0].x = x;
			tags[now_tag].smo_li[0].y = y;
            tags[now_tag].smo_li[0].t = time;
			tags[now_tag].smo_li[0].dt = 0.0;
			tags[now_tag].smo_num = 1;
			confidentDegree = -100.0;
			printf("-------手环%s初始化完成------- \n",id);
		}
		else
		{
			/*
			** 手环已经存在(flag=1)
			** 先判断手环是不是已经平稳(is_smo=1)
			** 再判断时间差是否在标准范围内，
			** 如果手环没有平稳，则进入判断是否平稳的函数
			** 如果手环已经平稳，则进入判断是否漂移的函数 
			*/
			
			/* 修改数据 将上一条记录改成当前条，当前条更新为读取的数据*/
			tags[i].pri_x = tags[i].cur_x;
			tags[i].pri_y = tags[i].cur_y;
			tags[i].pri_t = tags[i].cur_t;
			
			tags[i].cur_x = x;	
            tags[i].cur_y = y;
			tags[i].cur_t = time;	
			dt =(double) (tags[i].cur_t-tags[i].pri_t)/1000.0;       
			if(!tags[i].is_smo){
				//不平稳
				if(dt>=TIME_LIM){
					//时间差大于10s,清空准平稳段列表重新获取
					tags[i].smo_li[0].x = x;
					tags[i].smo_li[0].y = y;
					tags[i].smo_li[0].t = time;
					tags[i].smo_li[0].dt = dt;
					tags[i].smo_num = 1;
				}else{
					nn = tags[i].smo_num;
					tags[i].smo_li[nn].x = x;
					tags[i].smo_li[nn].y = y;
					tags[i].smo_li[nn].t = time;
					tags[i].smo_li[nn].dt = dt;
					tags[i].smo_num++;
					nn++;
					if(nn>=SMO_SIZE-1){
						//准平稳段已填满数据
						is_steady(i);
					}
				}
			}else{
				//平稳
				tags[i].cur_x = x;
				tags[i].cur_y = y;
				tags[i].cur_t = time;
				tags[i].pri_x = tags[i].cur_x;
				tags[i].pri_y = tags[i].cur_y;
				tags[i].pri_t = tags[i].cur_t;                
                char *binstr = coorToBin(x,y);
                tags[i].pri_decGrid = tags[i].cur_decGrid;
                tags[i].cur_decGrid = binToDec(binstr);
				strcpy(tags[i].pri_grid,tags[i].cur_grid);
                base32_encode(binstr,tags[i].cur_grid);
                // strcpy(tags[i].cur_grid,base32_encode(binstr));
				if(dt<TIME_LIM){
					//相邻数据在时间差范围内	
					confidentDegree = drift(i,dt)*100;
				}else{
					confidentDegree = 0;
				}
			}
			cJSON_AddNumberToObject(json,"cd",confidentDegree);
			char *jsonStr = cJSON_Print(json);
			printf("置信度%.2f \n",confidentDegree);
			return jsonStr;	
		}
		return data;
	}
}

char *ReadData(FILE *fp,char *buf)
{
	return fgets(buf,LINE,fp);
}

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
        return 0;
    }
    return -1;
}

int* connectedMatrix(int rN)
{
    int all = grids[rN].index;
    // int mat[all*all];
    int *mat;
    mat = (int *)malloc(all*all*sizeof(int));
    int i;
    int p;
    for(i = 0; i < all*all; i++)
        mat[i] = 0;
    for(i = 0; i < all; i++)
    {
        assignment(i,i,1,mat,all);//对角边
        if (grids[rN].areaGridArr[i].east == 0) {
            p = i + 1;/* 东 */
            if((p >= 0 && p < all) && grids[rN].areaGridArr[p].west == 0){
                assignment(i,p,1,mat,all);
                if(grids[rN].areaGridArr[i].north == 0){
                    p = i + pow(2,alpha);/* 北 */
                    if((p >= 0 && p < all) && grids[rN].areaGridArr[p].south == 0){
                        assignment(i,p,1,mat,all);
                        p = i + pow(2,alpha) + 1;/* 东北 */
                        assignment(i,p,1,mat,all);
                        if (grids[rN].areaGridArr[i].west == 0) {
                            p = i - 1;/* 西 */
                            if((p >= 0 && p < all) && grids[rN].areaGridArr[p].east == 0){
                                assignment(i,p,1,mat,all);
                                p = i + pow(2,alpha) - 1;/* 西北 */
                                assignment(i,p,1,mat,all);
                            }
                        }
                        
                    }
                }
                if(grids[rN].areaGridArr[i].south == 0){
                    p = i - pow(2,alpha);/* 南 */
                    if((p >= 0 && p < all) && grids[rN].areaGridArr[p].north == 0){
                        assignment(i,p,1,mat,all);
                        p = i - pow(2,alpha) + 1;/* 东南 */
                        assignment(i,p,1,mat,all);
                        if (grids[rN].areaGridArr[i].west == 0) {
                            p = i - 1;/* 西 */
                            if((p >= 0 && p < all) && grids[rN].areaGridArr[p].east == 0){
                                assignment(i,p,1,mat,all);
                                p = i + pow(2,alpha) - 1;/* 西北 */
                                assignment(i,p,1,mat,all);
                            }
                        }                        
                    }                    
                }
            }
        }else if(grids[rN].areaGridArr[i].west == 0){
            p = i - 1;/* 西 */
            if((p >= 0 && p < all) && grids[rN].areaGridArr[p].east == 0){
                assignment(i,p,1,mat,all);
                if(grids[rN].areaGridArr[i].north == 0){
                    p = i + pow(2,alpha);/* 北 */
                    if((p >= 0 && p < all) && grids[rN].areaGridArr[p].south == 0){
                        assignment(i,p,1,mat,all);
                        p = i + pow(2,alpha) - 1;/* 西北 */
                        assignment(i,p,1,mat,all);
                    }
                }
                if(grids[rN].areaGridArr[i].south == 0){
                    p = i - pow(2,alpha);/* 南 */
                    if((p >= 0 && p < all) && grids[rN].areaGridArr[p].north == 0){
                        assignment(i,p,1,mat,all);
                        p = i - pow(2,alpha) - 1;/* 西南 */
                        assignment(i,p,1,mat,all);
                    }
                }                                
            }
        }else{
            if(grids[rN].areaGridArr[i].north == 0){
                p = i + pow(2,alpha);/* 北 */
                if((p >= 0 && p < all) && grids[rN].areaGridArr[p].south == 0)
                    assignment(i,p,1,mat,all);
            }
            if(grids[rN].areaGridArr[i].south == 0){
                p = i - pow(2,alpha);/* 南 */
                if((p >= 0 && p < all) && grids[rN].areaGridArr[p].north == 0)
                    assignment(i,p,1,mat,all);
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

int canGet(void)
{
    int rN,row,i,j,count;
    for(rN = 0; rN < ROOMQUANTITY; rN++)
    {
        int *matA,*tmp,*matB;
        int accesstime;
        matA = connectedMatrix(rN);
        matB = matA;
        row = grids[rN].index;
        char *key1,*key2;
        int flag;
        for(int time = 1; time < STEP; time++)
        {
            tmp  = dot(matA,matB,row,row,row,row);
            accesstime = ((int)(time/4))+1;
            count = 0;
            for(i = 0; i < row; i++)
            {
                key1 = grids[rN].areaGridArr[i].numStr;
                
                for(j = 0; j < row; j++)
                {
                    if (tmp[i*row+j]>0)
                    {
                        key2 = grids[rN].areaGridArr[j].numStr;
                        hash_insert(key1,key2,accesstime);
                    }
                }
            }
            matB = tmp;
        }
    }
    return 0;
}


/*二分法*/
int dichotomy(double * range,double top,double low)
{
    double grid_size = (top-low)/2;
    int count = 1;
    while(*(range)>grid_size || grid_size>*(range+1))
    {
        // printf("%f\n",grid_size);
        // Sleep(100000);
        grid_size = grid_size/2;
        count++;
    }
    return count;
}

char* binGeohash(char* str, int count,int alpha)
{
    // char str[GEOLEN];
    itoa(count, str, 2);
    complement(str,alpha);
    return str;
}


char* geohash(char *str,int xCount,int yCount,int alpha)
{
    char xStr[GEOLEN],yStr[GEOLEN];
    binGeohash(xStr,xCount,alpha);
    binGeohash(yStr,yCount,alpha);
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


int getRoomNum(double x,double y,double xSize,double ySize)
{
    for (int rN = 0; rN < ROOMNUM; rN++)
    {
        if ((rooms[rN].low_x<=x+xSize && rooms[rN].top_x>x) && (rooms[rN].low_y<=y+ySize && rooms[rN].top_y>y))
            return rN;
    }
    return -1;
}

int inWall(double x,double y,double xSize,double ySize,int rN)
{
    if(rN >= 0)
    {
        int cant[] = {0,0,0,0,0};
        for(int wN = 0; wN < WALLNUM; wN++)
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
        // int j = 1;
        // int pos = 4;
        // int count = 0;
        // for (int i = 0; i < 4; i++)
        // {
        //     if (i==3)
        //         j = 0;
        //     if (cant[i]==1)
        //     {
        //         posList[count++] = i;
        //         if(cant[j]==1)
        //             posList[count++] = pos;
        //     }
        //     pos++;
        // }
        grids[rN].areaGridArr[grids[rN].index].east  = cant[0];
        grids[rN].areaGridArr[grids[rN].index].north = cant[1];
        grids[rN].areaGridArr[grids[rN].index].west  = cant[2];
        grids[rN].areaGridArr[grids[rN].index].south = cant[3];
        grids[rN].index++;
        return 0;
    }
    return 1;
}

int * geohash_grid(void)
{
    //动态计算精度范围
    double x_range[2]={0,0};//0:下界；1:上界;
    double y_range[2]={0,0};

    int xlim,ylim;
    x_range[0] = wall_x;
    y_range[0] = wall_y;
    if(door_x>door_y)
    {
        x_range[1] = door_x;
        alpha      = dichotomy(x_range,areas.top_x,areas.low_x);//通过x范围计算二分次数
    }
    else
    {
        y_range[1] = door_y;
        alpha      = dichotomy(y_range,areas.top_y,areas.low_y);//通过y范围计算二分次数
    }
    double xSize = (areas.top_x-areas.low_x)/pow(2,alpha);
    double ySize = (areas.top_y-areas.low_y)/pow(2,alpha);
    double x,y;
    int xCount,yCount;
    char binStr[GEOLEN];
    char geostr[GEOLEN];
    int count = 0;
    int index,rN;
    int *area_num;
    int index_lim = (pow(2,alpha))*(pow(2,alpha));
    area_num = (int *)malloc( index_lim *sizeof(int));
    int *passDire;
    // passDire = (int *)malloc( LINE *sizeof(int));
    // FILE *fp = NULL;
    // fp = fopen("D:\\program\\UWB\\data\\gridec.txt","w");
    for( yCount = 0; yCount < pow(2,alpha); yCount++)
    {
        y = areas.low_y + ySize * yCount;
        for( xCount = 0; xCount < pow(2,alpha); xCount++)
        {
            x = areas.low_x + xSize * xCount;
            geohash(binStr,xCount,yCount,alpha);
            base32_encode(binStr,geostr);
            index = binToDec(binStr);
            rN    = getRoomNum(x,y,xSize,ySize);
            // printf("%d\n",rN);
            area_num[index] = rN;
            if (rN>=0) {
                // puts("inroom!\n");
                grids[rN].areaGridArr[grids[rN].index].x = x;
                grids[rN].areaGridArr[grids[rN].index].y = y;
                grids[rN].areaGridArr[grids[rN].index].numDec = index;
                strcpy(grids[rN].areaGridArr[grids[rN].index].numStr,geostr);
                inWall(x,y,xSize,ySize,rN);
            }
            // fprintf(fp,"x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,binToDec(binStr));
            // printf("x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,index);
        }
        
    }
    // fclose(fp);
    return area_num;
}


















void *Read_Access_Area(void *arg)//改过
{
	/* 记录运行时间 */ 
	clock_t start,finish;
	double total_time;

	char *filename = (char *)arg;
	
	// init_area();
    int flags = 0;
    int count = 0;
    double * list;
    list  = (double *)malloc( LINE *sizeof(double));
    int num;
    double top_x  = -10000.0;
    double low_x  = 10000.0;
    double top_y  = -10000.0;
    double low_y  = 10000.0;
    double x;
    double y;
    int count_x = 0;
    int count_y = 0;
	FILE * fp = NULL;/*文件指针*/
    char * buf;/*数据缓存区，记得free*/
	buf = (char *)malloc(LINE*sizeof(char));
    fp = fopen(filename,"r+");/*房间区域数据文件*/
	start = clock();
	if (fp!=NULL) {
		puts("room.txt file reading...");
		while (fgets(buf,LINE,fp)!=NULL)
		{
			// puts(buf);
            if (strlen(buf)<=3)
            {
                rooms[flags].top_x = top_x;
                rooms[flags].low_x = low_x;
                rooms[flags].top_y = top_y;
                rooms[flags].low_y = low_y;
                flags++;
                count = 0;
                top_x = -10000.0;
                low_x = 10000.0;
                top_y = -10000.0;
                low_y = 10000.0;

            }
            else
            {
                split(buf,list);
                if (count== 0) {
                    rooms[flags].room_num = flags;
                    rooms[flags].door_num = (int) list[0];
                    rooms[flags].door_top_x = list[1]-COR_X;
                    rooms[flags].door_low_x = list[2]-COR_X;
                    rooms[flags].door_top_y = list[3]-COR_Y;
                    rooms[flags].door_low_y = list[4]-COR_Y;
                    door_x += list[1]-list[2];
                    door_y += list[3]-list[4];

                }
                else {
                    num = list[0];
                    rooms[flags].walls[num].top_x=list[1]-COR_X;
                    rooms[flags].walls[num].low_x=list[2]-COR_X;
                    rooms[flags].walls[num].top_y=list[3]-COR_Y;
                    rooms[flags].walls[num].low_y=list[4]-COR_Y;
                    rooms[flags].walls[num].wall_num = num;
                    top_x = max_double(top_x,list[1]-COR_X);
                    low_x = min_double(low_x,list[2]-COR_X);
                    top_y = max_double(top_y,list[3]-COR_Y);
                    low_y = min_double(low_y,list[4]-COR_Y);
                    x = list[1]-list[2];
                    y = list[3]-list[4];
                    // printf("x:y:%f,%f\n",x,y);
                    wall_x += x<y ? x:0;
                    wall_y += x>y ? y:0;
                    count_x +=x<y ? 1:0;
                    count_y +=x>y ? 1:0;
                    
                }
                count++;
            }            
		}
		fclose(fp);
		puts("room file close!");
	}else{
        /*文件读取错误！*/
		puts("room file error!");
		fclose(fp);
	} 
    
    door_x = door_x/flags;
    door_y = door_y/flags;
    wall_x = wall_x/count_x;
    wall_y = wall_y/count_y;   
    fp = fopen("D:\\program\\UWB\\data\\area.txt","r+");
    
    if (fp!=NULL) {
        fgets(buf,LINE,fp);
        puts("area.txt file reading...");
        split(buf,list);
        areas.top_x=list[0]-COR_X;
        areas.low_x=list[1]-COR_X;
        areas.top_y=list[2]-COR_Y;
        areas.low_y=list[3]-COR_Y;
        fclose(fp);
        puts("area file close!");
    }
    else {
        /*文件读取错误！*/
		puts("area file error!");
		fclose(fp);
    }
    fclose(fp);
    free(list);
    free(buf);
	areaGridDec = geohash_grid();	
	canGet();
	finish = clock();
	total_time = (double)(finish-start)/CLOCKS_PER_SEC;
	printf("Runnig time of this process:%0.4f ms \n",total_time);

}

/* 初始化 比如可达区域 */ 
int Init_Area()
{
	 
	pthread_mutex_init (&access_lock,NULL);
	
	
	pthread_t pid1;
	
	char file1[255];
	printf("Input the filename of room file(txt):");
	scanf("%s",&file1); 
	
	pthread_create(&pid1,NULL,Read_Access_Area,(void *)&file1);
 
		
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


int main()
{
    //正常
	if(!readConfiguration("config.json"))
		return 0;
	Init_HashTable();
	Init_Area();
	display_hash_table();
	
	/* 初始化线程池，开启THREAD_NUM条工作线程 */ 
	threadpool_t *pool = init(THREAD_NUM);	
	/* 直到接收线程停止，程序终止 */ 
	pthread_join(pool->rec_tid,NULL);
	
}

// main(int argc, char const *argv[])
// {

//     Read_Access_Area("room.txt")
//     return 0;
// }

