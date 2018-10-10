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
// #define SPEED_LIM 5	// 速度	 
#define LINE 41     /*txt文件行最大长度*/
#define ROOM_QUANT 13 /*房间区域数量*/
#define WALL_QUANT 8 /*房间内墙体数量*/
// #define COR_X 0.69/*修正x*/
// #define COR_Y 0.94/*修正y*/
#define COR_X 0.0/*修正x*/
#define COR_Y 0.0/*修正y*/
#define GEO_BIN_LEN 14/*Geohash二进制字符串*/
#define GEO_STR_LEN 3/*Geohash字母字符串长度*/
#define BASE32_LAY_LEN 5 /*Geohash字符编码最大长度*/
#define BASE32_MIN_LEN 8 /*BASE32每层网络的字符长度，多少个二进编码为一个字符*/
#define GRID_QUANT 20000 /*栅格数量*/
#define STEP 30 /*步数*/
#define SMO_SIZE 10
#define TIME_LIM 10
#define SPEED_LIM 4
#define ROUTE_QUANT 50

static const char base32_alphabet[32] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'j', 'k', 'm', 'n', 'p', 'q', 'r',
        's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

/*路由表*/
typedef struct Route{
	int SourAddr;
	int TargAddr;
	double dist;
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
typedef struct Smo{
	double x;//当前
	double y;
	double dt;
	long int t;
};



/*手环*/
typedef struct Tag{
	char *id = NULL;	/* 手环编号 */ 
	double cur_x;	//当前x 
	double cur_y;	// 当前y 
	long int cur_t;	//当前数据时间 
	char *cur_grid;	//当前区域编号 
	double pri_x;	//上一x 
	double pri_y;	//上一y 
    long long pri_t;	//上一时间
    char *pri_grid;	//上一区域编号
    int is_smo=0;	//是否平稳， 1为平稳，0为非平稳 
    struct Smo smo_li[SMO_SIZE];		//10条准平稳数据 
    int smo_num=0;	//已存放平稳数据,若为0则表示该手环没初始化过 
    int cur_decGrid; 
    int pri_decGrid;
};

/*墙*/
typedef struct Wall{
    double top_x;
	double low_x;
	double top_y;
	double low_y;
};

typedef struct Room{
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
    struct Wall walls[WALL_QUANT];
	int grid_num;/*房间内栅格数量*/
	int grid_list[300];
};

typedef struct Area{
    double top_x;
    double low_x;
    double top_y;
    double low_y;
};

typedef struct Grid{
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


/* 当前拥有的哈希节点数目 */ 
int hash_table_size;
HashNode* hashTable[HASH_MAX_SIZE];

/* access_lock */
pthread_mutex_t access_lock;

/* 本地创建的客户端tcp_socket */
static SOCKET tcp_client;
/* 用于存储服务器的基本信息 */
static struct sockaddr_in tcp_server_in;
struct Room rooms[ROOM_QUANT];/*房间区域数组*/
struct Grid grids[GRID_QUANT];/*栅格数组*/
struct Area areas;
struct Tag tags[TAG_SIZE];/* 手环数组 */ 
int alpha;
double xSize,ySize;/*栅格尺寸*/
int area_num[GRID_QUANT];
int now_tag = 0;
struct Route route_table[ROUTE_QUANT];/*路由表数组*/



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

double Distf(double x1,double y1,double x2,double y2){
	return pow((pow((x1-x2),2)+pow((y1-y2),2)),0.5);
}

int Init_Route(void){
	int i,j,count = 0;
	
	for (i = 0; i < ROOM_QUANT; i++)
	{
		for (j = 0; j < ROOM_QUANT; j++)
		{
			if (i != j)
			{
				route_table[count].SourAddr = i;
				route_table[count].TargAddr = j;
				route_table[count].dist = Distf(rooms[i].door_cer_x,rooms[i].door_cer_y,rooms[j].door_cer_x,rooms[j].door_cer_y);/*待改*/
				count++;
			}
		}
	}
}

double Search_Route(int rN1,int rN2){
	int i;
	for(i = 0; i < ROUTE_QUANT; i++)
	{
		if ((route_table[i].SourAddr == rN1 && route_table[i].TargAddr ==rN2) || 
		(route_table[i].SourAddr == rN2 && route_table[i].TargAddr ==rN1))
		{
			return route_table[i].dist;
		}
	}
	return 0.0;
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



/* 求速度 */ 
// float  speed(smo s1,smo s2)
// {
// 	float diff_time = (s2.t-s1.t)/1000.0;
// 	float dist = pow(pow(s1.x-s2.x,2.0)+pow(s1.y-s2.y,2.0),0.5);
// 	float sp = dist/diff_time;
// 	return sp;
// }
double speed(double dist,double dt){
    if (dt==0 and dist != 0)
    {
        return dist;
    }else if  (dt==0 and dist == 0){
        return 1;
    }else
        return dist/dt;
}

double dists(Smo s1,Smo s2){
    return (double) (pow(pow(s1.x-s2.x,2.0)+pow(s1.y-s2.y,2.0),0.5));
}

/*二进制转十进制*/
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

/*  输入x,y坐标，输出二进制字符串
**	@param x doublex
**	@param y doubley
*/
int coorToBin(double x, double y, char *strBin)
{
	// puts("coorToBin");
    double x_mid,y_mid;
    double x_min = areas.low_x;
    double y_min = areas.low_y;
    double x_max = areas.top_x;
    double y_max = areas.top_y;
    if ((x>=x_min && x<=x_max) && (y>=y_min && y<=y_max))
    {
        // puts("In area.");
        for(int i = 0; i < alpha; i++)
        {
            y_mid = (y_min + y_max) / 2.0;
            x_mid = (x_min + x_max) / 2.0;
			// printf("->xm:%f,ym:%f\n\n",x_mid,y_mid);
            if (x_min<= x && x<x_mid) {
                strBin[i*2] = '0';/* 左0 */
				// printf("0:%f<=x:%f<%f\n\n",x_min,x,x_mid);
                x_max = x_mid;
            }
            else {
                strBin[i*2] = '1';/* 右1 */
				// printf("1:%f<=x:%f<%.9f\n\n",x_mid,x,x_max);
                x_min = x_mid;
            }
            if (y_min<=y && y<y_mid) {
                strBin[i*2+1] = '0';/* 左0 */
				// printf("0:%f<=y:%f<%f\n\n",y_min,y,y_mid);
                y_max = y_mid;
            }
            else {
                strBin[i*2+1] = '1';/* 右1 */
				// printf("1:%f<=y:%f<%f\n\n",y_mid,y,y_max);
                x_min = x_mid;
            }
			// puts(strBin);
			
        }
        strBin[alpha*2] = '\0';
        return 0;
    }else{
        puts("Out of area!");
        return 1;
    }     
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


/*用base32将二进制字符串转成字符串*/
char* base32_encode(char *bin_source,char * code)
{
    char *tmpchar;
    int   num;
    int   count = 0;
    int   codeDig;
    tmpchar   = (char *)malloc( BASE32_LAY_LEN *sizeof(char));
    complement(bin_source,BASE32_MIN_LEN);/*若二进制字符串长度小于8，补位*/
    for(int i = 0; i < strlen(bin_source) ;  i+=BASE32_LAY_LEN)
    {	
        strncpy(tmpchar, bin_source+i, BASE32_LAY_LEN);
        tmpchar[BASE32_LAY_LEN]= '\0';
        complement(tmpchar,BASE32_LAY_LEN);/*若最后一层字符串长度小于5，补位*/
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

/* 判断手环数据是否平稳 
** @param tag_pos int,手环在手环数组的下标 
*/
int is_steady(int tag_pos)
{
    int i;
    int count=0;
    double sp,dt;
    char binstr[alpha*2 + 1];
    for(i=1;i<SMO_MAX_NUM;i++)
    {
        // speed(tags[tag_pos].smo_li[i-1].dt)
        double sp = speed(dists(tags[tag_pos].smo_li[i-1],tags[tag_pos].smo_li[i]),
        tags[tag_pos].smo_li[i-1].dt);
        if(sp<SPEED_LIM)
        {
            count++;
            if(count==7)
            {	
                printf("手环%s准平稳数组内已经有7条平稳数据\n",tags[tag_pos].id);
                tags[tag_pos].is_smo=1;
                if(coorToBin(tags[tag_pos].cur_x,tags[tag_pos].cur_y,binstr)==1){
					printf("手环%s准平稳数组内最后一条数据不在可通行范围内，重新寻找平稳段\n",tags[tag_pos].id);
                    tags[tag_pos].is_smo=0;
                    tags[tag_pos].smo_num = 0;
                    return 1;
                }
                else {
					printf("手环%s已经平稳\n",tags[tag_pos].id);
                    tags[tag_pos].cur_decGrid = binToDec(binstr);
					tags[tag_pos].cur_grid = (char *)malloc(GEO_STR_LEN*sizeof(char));
					tags[tag_pos].pri_grid = (char *)malloc(GEO_STR_LEN*sizeof(char));
                    base32_encode(binstr,tags[tag_pos].cur_grid);
                    return 0;
                }
                

            }
        }
    }
    printf("/n");
    tags[tag_pos].smo_num = 0;
    return 1;
}


/*时间戳处理*/
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







float drift(int tag_pos,int dt){
    int area_cur = area_num[tags[tag_pos].cur_decGrid];
    int area_pri = area_num[tags[tag_pos].cur_decGrid];
    char * key   = tags[tag_pos].pri_grid;
    char * askey = tags[tag_pos].cur_grid;
    if(area_cur < 0 || area_pri < 0 ){
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
                    return 0.2;
            }
            as = as->next;
        }
    }else{
		double sp = Search_Route(area_cur,area_pri)/dt;
		
		if (sp<=SPEED_LIM) {
			return 1;
		}
		else {
			return 0;
		}
		
        return 0;
    }

}


/* 解析传来的字符串 */ 
char * resolve_str(char *data)
{
    /* 用cJSON解析 */ 
    cJSON *json = cJSON_Parse(data);
    if(json)	//解析成功  
    {
        int flag = 0;/* 判断是否已初始化该手环，0为无，1为有 */
        int i,nn;/* i:手环在手环数组里的下标 */ 	
        double dt;/*时间差*/
        double confidentDegree;/*置信度*/
        /* 解析数据 */
        char *id = cJSON_GetObjectItem(json, "tagid")->valuestring;	/* id */ 
        double x = cJSON_GetObjectItem(json, "x")->valuedouble;	/* x */ 
        double y = cJSON_GetObjectItem(json, "y")->valuedouble;	/* y */ 
        char *time_str = cJSON_GetObjectItem(json, "time")->valuestring;	/* time */ 
        long long time = timestamp(time_str);
        /* 在手环数组寻找手环 */ 
        for(i=0;i<TAG_SIZE;i++)
        {
			if (tags[i].id==NULL) {
				break;
			}
            if(strcmp(tags[i].id,id)==0)
            {
                flag = 1;//若存在则跳出
                printf("手环%s存在于数组%d上 \n",id,i);
                break;		
            }

        }
        if(flag==0)
        {
            printf("-------正在为手环%s初始化------- \n",id);
			tags[now_tag].id = (char *)malloc(strlen(id)*sizeof(char));
            strcpy(tags[now_tag].id,id);//id赋值
            tags[now_tag].cur_x = x;
            tags[now_tag].cur_y = y;
            tags[now_tag].cur_t = time;
            tags[now_tag].smo_li[0].x = x;
            tags[now_tag].smo_li[0].y = y;
            tags[now_tag].smo_li[0].t = time;
            tags[now_tag].smo_li[0].dt = 0.0;
            tags[now_tag].smo_num = 1;
            confidentDegree = -100.0;
            now_tag ++;
            printf("-------手环%s初始化完成------- \n",id);
        }
        else
        {
			/* 
			** 先判断手环是不是已经平稳（即字段is_smo是否等于1） 
			** 如果手环没有平稳，则进入判断是否平稳的函数
			** 如果手环已经平稳，则进入判断是否漂移的函数 
			** 修改数据 将上一条记录改成当前条，当前条更新为读取的数据
			*/ 			
			tags[i].pri_x = tags[i].cur_x;
            tags[i].pri_y = tags[i].cur_y;
            tags[i].pri_t = tags[i].cur_t;
            tags[i].cur_x = x;	
            tags[i].cur_y = y;
            tags[i].cur_t = time;	
            dt =(double) (tags[i].cur_t-tags[i].pri_t)/1000.0;   
            if(!tags[i].is_smo){
				printf("-------手环%s未进入平稳状态------- \n",id);
                if(dt>=TIME_LIM){
                    //时间差大于等于10s
                    puts("相邻时间差大于10s");
                    tags[i].smo_li[0].x = x;
                    tags[i].smo_li[0].y = y;
                    tags[i].smo_li[0].t = time;
                    tags[i].smo_li[0].dt = dt;
                    tags[i].smo_num = 1;
                }else{
                    puts("相邻时间差小于10s");
                    nn = tags[i].smo_num;
                    tags[i].smo_li[nn].x = x;
                    tags[i].smo_li[nn].y = y;
                    tags[i].smo_li[nn].t = time;
                    tags[i].smo_li[nn].dt = dt;
                    tags[i].smo_num++;
                    nn++;
                    if(nn>=SMO_SIZE){
						printf("手环%s准平稳段已经装满10条数据，开始判断其平稳性与否...\n",id);
                        is_steady(i);
                    }
                }
            }else{
				printf("手环%s已经平稳\n",id);
                char binstr[GEO_BIN_LEN];
                tags[i].cur_x = x;
                tags[i].cur_y = y;
                tags[i].cur_t = time;
                tags[i].pri_x = tags[i].cur_x;
                tags[i].pri_y = tags[i].cur_y;
                tags[i].pri_t = tags[i].cur_t;    
                if (coorToBin(x,y,binstr)==0) {
                    tags[i].pri_decGrid = tags[i].cur_decGrid;
                    tags[i].cur_decGrid = binToDec(binstr);
                    strcpy(tags[i].pri_grid,tags[i].cur_grid);
                    base32_encode(binstr,tags[i].cur_grid);
                }else {
                    confidentDegree = -100;
                }
                // strcpy(tags[i].cur_grid,base32_encode(binstr));
                if(dt<TIME_LIM){
                    puts("时间差小于10s");
                    confidentDegree = drift(i,dt)*100;
                }else{
                    puts("时间差大于10s");
                    confidentDegree = 0;
                }
            }

        }
        cJSON_AddNumberToObject(json,"cd",confidentDegree);
        char *jsonStr = cJSON_Print(json);
        printf("置信度=%.2f \n",confidentDegree);
        printf("\n");
        return jsonStr;	
    }
    // puts("pp");
    
    return data;
}


char *ReadData(FILE *fp,char *buf)
{
	return fgets(buf,LINE,fp);
}




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

void *Area_information_acquisition(void *arg){
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
		printf("--->top_x=%f,low_x=%f,top_y=%f,low_y=%f\n",areas.top_x,areas.low_x,areas.top_y,areas.low_y);
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
	printf("grid width:%f\n",grid_size);
    return count;
}

void *Room_information_acquisition(void *arg)
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
			if (buf[0]=='#'){
				if (count=4){
					rooms[rN].room_type=0;/*标准区域*/
				}else{
					rooms[rN].room_type=1;/*非区域*/
				}
				printf("--->room%d:top_x=%f,low_x=%f,top_y=%f,low_y=%f\n",rN,rooms[rN].top_x,rooms[rN].low_x,rooms[rN].top_y,rooms[rN].low_y);/*房间区域坐标打印*/
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
	// printf("x:%f,%f\n",x_range[0],x_range[1]);
	// printf("y:%f,%f\n",y_range[0],y_range[1]);
	//动态计算精度范围
    if(x_range[1]>y_range[1]){
        alpha      = dichotomy(x_range,areas.top_x,areas.low_x);//通过x范围计算二分次数
    }else{
        alpha      = dichotomy(y_range,areas.top_y,areas.low_y);//通过y范围计算二分次数
    }
	printf("二分次数：%d\n",alpha);
	// sleep(1000);
	return 0;
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
	// puts("Geohash_segGrid_Bin");
    char xStr[GEO_BIN_LEN/2],yStr[GEO_BIN_LEN/2];
    DectoBin(xStr,xCount,alpha);
    DectoBin(yStr,yCount,alpha);
    // char str[GEOLEN];
	// puts(xStr);
	// puts(yStr);
    int j;
    for(int i = 0; i < alpha*2; i++)
    {
        str[i]=i/2;
        j = i/2;
        if (i%2==0) 
            str[i]=xStr[j];/* 偶x */
        else 
            str[i]=yStr[j];/* 奇 */
		// puts(str);
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
	// printf("x=%f,y=%f,bx=%f,by=%f\n",x,y,x+xSize,y+ySize);
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
    int index_lim = (pow(2,alpha))*(pow(2,alpha));
	FILE *fp = fopen("log.txt","w");
	FILE *fp1 = fopen("rinf.txt","w");
    for( yCount = 0; yCount < pow(2,alpha); yCount++)
    {
        y = areas.low_y + ySize * yCount + ySize/2;
        for( xCount = 0; xCount < pow(2,alpha); xCount++)
        {
            x = areas.low_x + xSize * xCount+xSize/2;
			coorToBin(x,y,binStr);
			// BintoDec(binstr);
			printf("x=%f,y=%f,%s\n",x,y,binStr);
			// sleep(1);
            // Geohash_segGrid_Bin(binStr,xCount,yCount,alpha);
            Base32_BintoStr(binStr,geostr);
            index = BintoDec(binStr);
            rN    = GetRoomNum(x,y);
			fprintf(fp1,"x=%f,y=%f,rN=%d\n",x,y,rN);
            area_num[index] = rN;
            if (rN != -1) {
				fprintf(fp,"x:%f,y:%f,xc:%d,yc:%d == bin=%s,Geohash=%s,decimal base=%d,in room = %d,room grid num = %d,east=%d,south=%d,west=%d,north=%d,\n",x,y,xCount,yCount,binStr,geostr,index,rN,rooms[rN].grid_num,grids[index].east,grids[index].south,grids[index].west,grids[index].north);
				strcpy(grids[index].numStr,geostr);
				grids[index].numDec = index;
				// printf("x = %f,y= %f,in room num:%d\n",x,y,rN);
				grids[index].areaNum   = rN;
				grids[index].x         = x;
				grids[index].y         = y;	
				grids[index].areaCount = rooms[rN].grid_num;
				rooms[rN].grid_list[rooms[rN].grid_num]=index;
				
                inWall(x,y,rN,index);
				
				rooms[rN].grid_num ++;

            }
            // printf("x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,index);
        }
        
    }
	fclose(fp1);
	fclose(fp);  
    return area_num;
}

/* 读取txt*/ 
int Init_Read_txt()
{
	pthread_mutex_init (&access_lock,NULL);
	pthread_t pid1;
	pthread_t pid2;
	char file1[255];
	char file2[255];
	printf("Input the filename of all area information txt:");
	scanf("%s",&file2); 
	printf("file2=%s\n",file2);
	printf("Input the filename of room information txt:");
	scanf("%s",&file1);
	printf("file1=%s\n",file1);
	pthread_create(&pid2,NULL,Area_information_acquisition,(void *)&file2);
	pthread_create(&pid1,NULL,Room_information_acquisition,(void *)&file1);

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
		// printf("%d\n",index);
        return 0;
    }
    return -1;
}




int CoortoDec(int x , int y){
	char str[GEO_BIN_LEN];
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


int* connectedMatrix(int rN,int *mat){
	int all  = rooms[rN].grid_num;
	// int * mat;
	// mat = (int *)malloc(all*all*sizeof(int));
	int i,j,k;
	int i_mat,j_mat,i_grid,j_grid;
	// printf("all:%d\n",all);
	for(i = 0; i < all*all; i++)
        mat[i] = 0;

	for(i_mat = 0; i_mat < all; i_mat++){
		// printf("%d\n",i_mat);
		assignment(i_mat,i_mat,1,mat,all );
		i_grid = rooms[rN].grid_list[i_mat];
		printf("--->Geohash=%s,Dec=%d,roomCount=%d,x=%f,y=%f\n",grids[i_grid].numStr,grids[i_grid].numDec,grids[i_grid].areaCount,grids[i_grid].x,grids[i_grid].y);
		if (grids[i_grid].east==0) {
			j_grid = CoortoDec(grids[i_grid].x+xSize,grids[i_grid].y);/*东*/
			printf("---east--->x:%f,y:%f;rN=%d\n",grids[i_grid].x+xSize,grids[i_grid].y,CheckArea(j_grid));

			printf("i:%d,j:%d\n",i_grid,j_grid);
			if(CheckArea(j_grid)==rN && grids[j_grid].west==0){
				j_mat = grids[j_grid].areaCount;
				assignment(i_mat,j_mat,1,mat,all);
				if (grids[i_grid].north==0) {
					j_grid = CoortoDec(grids[i_grid].x,grids[i_grid].y+ySize);/*北*/
					if(CheckArea(j_grid)==rN && grids[j_grid].south==0){
						j_mat = grids[j_grid].areaCount;	
						assignment(i_mat,j_mat,1,mat,all);
						j_grid = CoortoDec(grids[i_grid].x+ySize,grids[i_grid].y+ySize);/*东北*/
						j_mat = grids[j_grid].areaCount;
						assignment(i_mat,j_mat,1,mat,all);
						if(grids[i_grid].west==0) {
							j_grid = CoortoDec(grids[i_grid].x-xSize,grids[i_grid].y);/*西*/
							if(CheckArea(j_grid)==rN && grids[j_grid].east==0){
								j_mat = grids[j_grid].areaCount;	
								assignment(i_mat,j_mat,1,mat,all);
								j_grid = CoortoDec(grids[i_grid].x-ySize,grids[i_grid].y+ySize);/*西北*/
								j_mat = grids[j_grid].areaCount;
								assignment(i_mat,j_mat,1,mat,all);									
							}
						}							
					}					
				}
				if (grids[i_grid].south==0) {
					j_grid = CoortoDec(grids[i_grid].x,grids[i_grid].y-ySize);/*南*/
					if(CheckArea(j_grid)==rN && grids[j_grid].north==0){
						j_mat = grids[j_grid].areaCount;	
						assignment(i_mat,j_mat,1,mat,all);
						j_grid = CoortoDec(grids[i_grid].x+ySize,grids[i_grid].y-ySize);/*东南*/
						j_mat = grids[j_grid].areaCount;
						assignment(i_mat,j_mat,1,mat,all);
						if(grids[i_grid].west==0) {
							j_grid = CoortoDec(grids[i_grid].x-xSize,grids[i_grid].y);/*西*/
							if(CheckArea(j_grid)==rN && grids[j_grid].east==0){
								j_mat = grids[j_grid].areaCount;	
								assignment(i_mat,j_mat,1,mat,all);
								j_grid = CoortoDec(grids[i_grid].x-ySize,grids[i_grid].y-ySize);/*西南*/
								j_mat = grids[j_grid].areaCount;
								assignment(i_mat,j_mat,1,mat,all);									
							}
						}							
					}					
				}					
			}
		}
		else if(grids[i_grid].west==0) {
			j_grid = CoortoDec(grids[i_grid].x-xSize,grids[i_grid].y);/*西*/
			if(CheckArea(j_grid)==rN && grids[j_grid].east==0){	
				j_mat = grids[j_grid].areaCount;	
				assignment(i_mat,j_mat,1,mat,all);
				if (grids[i_grid].north==0) {
					j_grid = CoortoDec(grids[i_grid].x,grids[i_grid].y+ySize);/*北*/
					if(CheckArea(j_grid)==rN && grids[j_grid].south==0){
						j_mat = grids[j_grid].areaCount;	
						assignment(i_mat,j_mat,1,mat,all);
						j_grid = CoortoDec(grids[i_grid].x-ySize,grids[i_grid].y+ySize);/*西北*/
						j_mat = grids[j_grid].areaCount;
						assignment(i_mat,j_mat,1,mat,all);						
					}					
				}
				if (grids[i_grid].south==0) {
					j_grid = CoortoDec(grids[i_grid].x,grids[i_grid].y-ySize);/*南*/
					if(CheckArea(j_grid)==rN && grids[j_grid].north==0){
						j_mat = grids[j_grid].areaCount;	
						assignment(i_mat,j_mat,1,mat,all);
						j_grid = CoortoDec(grids[i_grid].x-ySize,grids[i_grid].y-ySize);/*西南*/
						j_mat = grids[j_grid].areaCount;
						assignment(i_mat,j_mat,1,mat,all);							
					}					
				}									
			}		
		}
		else {
			if (grids[i_grid].north==0) {
				j_grid = CoortoDec(grids[i_grid].x,grids[i_grid].y+ySize);/*北*/
				if(CheckArea(j_grid)==rN && grids[j_grid].south==0){
					j_mat = grids[j_grid].areaCount;	
					assignment(i_mat,j_mat,1,mat,all);					
				}					
			}
			if (grids[i_grid].south==0) {
				j_grid = CoortoDec(grids[i_grid].x,grids[i_grid].y-ySize);/*南*/
				if(CheckArea(j_grid)==rN && grids[j_grid].north==0){
					j_mat = grids[j_grid].areaCount;	
					assignment(i_mat,j_mat,1,mat,all);						
				}					
			}
		}
	}
}




int * dot(int * a_mat, int * b_mat,int a_row,int b_row,int a_col,int b_col,int *c_mat)
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
        // int *c_mat;
        // c_mat = (int *)malloc(a_row * b_col*sizeof(int));
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

int test(void){
	int *matA;
    matA = (int *)malloc(rooms[0].grid_num*rooms[0].grid_num*sizeof(int));
	connectedMatrix(0,matA);
	// printf("%d\n",matA[0]);
	return 0;
}

int canGet(void){
	int rN,accesstime,row,i,j,i_grid,j_grid;
	int *matA,*tmp,*matB;
    char *key1,*key2;
	puts("####################开始计算可达表####################");	
	for(rN = 0; rN < 1; rN++)
	{
		matA = (int *)malloc(rooms[rN].grid_num*rooms[rN].grid_num*sizeof(int));
		connectedMatrix(rN,matA);
		row = rooms[rN].grid_num;
		// FILE *fp = fopen("mat.txt","w");
		// fputs("a",fp);
		// if (rN==0)
		// {
		// 	for(int ip = 0; ip < row*row; ip++)
		// 	{
		// 		printf("%d,",matA[ip]);
		// 		if ((ip+1)%row==0)
		// 		{
		// 			printf("##\n");
		// 		}
		// 	}
			
		// }
		// fclose(fp);
		// printf("%d\n",row);
		// for(int time = 0; time < STEP; time++){
			
		// 	if (time==0) {
		// 		matB = (int *)malloc(row*row*sizeof(int));
		// 		tmp  = (int *)malloc(row*row*sizeof(int));
		// 		memcpy(matB,matA,sizeof(int)*row*row);
		// 		memcpy(tmp ,matA,sizeof(int)*row*row);
		// 	}
		// 	else {
		// 		dot(matA,matB,row,row,row,row,tmp);
		// 		memcpy(matB,tmp,sizeof(int)*row*row);
		// 	}
		// 	accesstime = ((int)(time/4))+1;
		// 	for(i = 0; i < row; i++){
		// 		for(j = 0; j < row; j++){
		// 			i_grid = rooms[rN].grid_list[i];
		// 			key1 = grids[i_grid].numStr;
        //             if (i != j && tmp[i*row+j]>0){
		// 				j_grid = rooms[rN].grid_list[j];
		// 				key2 = grids[j_grid].numStr;
		// 				// printf("%s,%s\n",key1,key2);
		// 				// printf("%d,%d\n",i_grid,j_grid);
		// 				hash_insert(key1,key2,accesstime);
		// 				// sleep(1000);
		// 			}					
		// 		}				
		// 	}
		// }
		// free(tmp);
		// free(matA);
		// free(matB);
		
	}
	puts("####################可达表计算结束####################");
	

}

int canGet1(void){
	int rN,accesstime,row,i,j,i_grid,j_grid;
	int *matA,*tmp,*matB;
    char *key1,*key2;
	puts("####################开始计算可达表####################");	
	for(rN = 0; rN < ROOM_QUANT; rN++)
	{
		matA = (int *)malloc(rooms[rN].grid_num*rooms[rN].grid_num*sizeof(int));
		connectedMatrix(rN,matA);
		row = rooms[rN].grid_num;
		// FILE *fp = fopen("mat.txt","w");
		// fputs("a",fp);
		// if (rN==0)
		// {
		// 	for(int ip = 0; ip < row*row; ip++)
		// 	{
		// 		printf("%d,",matA[ip]);
		// 		if ((ip+1)%row==0)
		// 		{
		// 			printf("##\n");
		// 		}
		// 	}
			
		// }
		// fclose(fp);
		// printf("%d\n",row);
		for(int time = 0; time < STEP; time++){
			
			if (time==0) {
				matB = (int *)malloc(row*row*sizeof(int));
				tmp  = (int *)malloc(row*row*sizeof(int));
				memcpy(matB,matA,sizeof(int)*row*row);
				memcpy(tmp ,matA,sizeof(int)*row*row);
			}
			else {
				dot(matA,matB,row,row,row,row,tmp);
				memcpy(matB,tmp,sizeof(int)*row*row);
			}
			accesstime = ((int)(time/4))+1;
			for(i = 0; i < row; i++){
				for(j = 0; j < row; j++){
					i_grid = rooms[rN].grid_list[i];
					key1 = grids[i_grid].numStr;
                    if (i != j && tmp[i*row+j]>0){
						j_grid = rooms[rN].grid_list[j];
						key2 = grids[j_grid].numStr;
						// printf("%s,%s\n",key1,key2);
						// printf("%d,%d\n",i_grid,j_grid);
						hash_insert(key1,key2,accesstime);
						// sleep(1000);
					}					
				}				
			}
		}
		free(tmp);
		free(matA);
		free(matB);
		
	}
	puts("####################可达表计算结束####################");
	

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
int remove(int index , char * data)
{
    char * tmp =NULL;
    tmp = (char*)malloc(strlen(data)*sizeof(char));
    strcpy(tmp,data);
    for(int i = index; i < (strlen(data)-1); i++)
    {
        data[i] = tmp[i+1];
    }
    data[strlen(data)-1] = '\0';
    free(tmp);
    return 0;
}




int main()
{
	if(!readConfiguration("config.json"))
		return 0;
	
	Init_HashTable();
	// Init_Read_txt();
	char areafile[] = "areananshan.txt";
	char roomfile[] = "roomnanshan.txt";
	// char areafile[255];
	// char roomfile[255];
	// printf("Input the filename of all area information txt:");
	// scanf("%s",&areafile); 
	// printf("Input the filename of all room information txt:");
	// scanf("%s",&roomfile); 
	Area_information_acquisition((void *)&areafile);
	Room_information_acquisition((void *)&roomfile);
    xSize = (areas.top_x-areas.low_x)/pow(2,alpha);
    ySize = (areas.top_y-areas.low_y)/pow(2,alpha);	
	printf("area:low_x=%f,low_y=%f,top_x=%f,top_y=%f\n",areas.low_x,areas.low_y,areas.top_x,areas.top_y);
	Geohash_Grid();
	// canGet(); 
	// test();
	// puts("Geohash=75f,Dec=3678,roomCount=25,x=0.455297,y=-8.082750");
	// printf("rN = %d\n",CoortoDec(0.455297,-8.082750));
	// puts("x:26.353500,y:-21.854750,xc:96,yc:14");
	char s1[10];
	char s2[10];
	coorToBin(0.743055,-23.361063,s1);
	puts("#####################");
	coorToBin(1.318570,-23.361063,s2);
	Geohash_segGrid_Bin(s2,96,14,alpha);
	puts("");
	printf("0.743055,-23.361063;%s\n",s1);
	printf("1.318570,-23.361063;%s\n",s2);
	// for(int i = 0; i < ROOM_QUANT; i++)
	// {
	// 	printf("room%d has %d grid\n",i,rooms[i].grid_num);
	// }
	
	// Init_Route();

	// display_hash_table();
	
	// /* 初始化线程池，开启THREAD_NUM条工作线程 */ 
	// threadpool_t *pool = init(THREAD_NUM);	
	// /* 直到接收线程停止，程序终止 */ 
	// pthread_join(pool->rec_tid,NULL);
	
}





// int main(int argc, char const *argv[])
// {
// 	clock_t start,finish;
// 	double total_time;
// 	char filename1[] = "area.txt";
// 	char filename2[] = "room.txt";
// 	Area_information_acquisition((void *)&filename1);
// 	Room_information_acquisition((void *)&filename2);
// 	Geohash_Grid();
// 	Init_HashTable();
// 	canGet(); 
// 	Init_Route();
// 	puts("init ok;");
//     char data[] = "[{\"type\":0,\"time\":\"2018-7-25 16:14:30.354\",\"le_guid\":\"\",\"tagid\":\"DECA00FFCF54581C\",\"xcoord\":4.170346,\"ycoord\":2.147416,\"zcoord\":10000}]";
//     remove(0,data);
// 	remove(134,data);
// 	// puts(data);
//     for(int i = 0; i < 20; i++)
//     {
//         start = clock(); 
//         resolve_str(data);
//         finish = clock(); 
//         total_time = (double)(finish-start)/CLOCKS_PER_SEC;
//         printf("total_time=%fs\n\n################\n",total_time);
//     }
    
    
//     return 0;
// }


