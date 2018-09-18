#include <stdio.h>
// #include <pthread.h>
#include <winsock2.h>
#include <stdlib.h>
#include <unistd.h>	
// #include <sched.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "cJSON.c"
//#include <stdio.h>

#define test_char "[{\"type\":0,\"time\":\"2018-7-25 16:14:30.354\",\"le_guid\":\"\",\"tagid\":\"DECA00FFCF54581C\",\"xcoord\":4.170346,\"ycoord\":2.147416,\"zcoord\":10000}]"
#define LINE 138
#define TAG_SIZE 100
#define TIME_LIM 10
#define SMO_SIZE 10
#define SP_LIM 4.0

/*函数体*/
/*1.准平稳段*/
typedef struct smo{
	double x;//当前
	double y;
	long long t;
    double dt;
};


/*2.手环缓存区*/
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
};

/* 3.可达区域 */ 
typedef struct Access{	
	char *key;
	int time = 0;
	Access *next;
};

/* 4.哈希节点 */ 
typedef struct HashNode{
	char *key;
	/* 可达区域数组 这里是将1、3、5秒的都写在这里，也可以开三个数组 */
	Access *access;	

	HashNode *next;	/* 下一个节点 */ 
};

typedef struct Elem{
	char *elem;
};

//全局变量区
// FILE * fp = fopen("18072501.txt","r+");
Tag tags[TAG_SIZE];//手环数组，100个
int now_tag = 0;


/*声明区*/
void getdata(FILE * fp, char * data);
long long timestamp(char * time_str);
void resolve_str(char *data);

typedef struct smo;
typedef struct Tag;
int main(void)
{
    FILE * fp = fopen("18072501.txt","r+");
    char * data;
    data = (char *)malloc(LINE*sizeof(char));
    // while(fp != 0){
    //     getdata(fp,data);

    // }  
    getdata(fp,data);
    puts(data);
    // resolve_str(data);
    // char p[] = "2018-7-25 16:14:30.354";
    // // timestamp(p);
    // long long ps = timestamp(p);
    // printf("%lld",ps);
    // resolve_str1(data);
	// char *time_str = cJSON_GetObjectItem(json, "time")->valuestring;	/* 时间 */ 
	// puts(time_str);
    getchar();
    return 0;
}

/*函数部分*/
/*1.获取数据*/
void getdata(FILE * fp, char * data){
    fgets(data,LINE,fp);
    // puts(data);
    while(strlen(data)<30){
        fgets(data,LINE,fp);
    }
    // /*把尾括号去掉*/
    int last = strlen(data);
    // data[last] = '\0';
    /*把方括号去掉*/
    int i;
    for(i = 0; i < last; i++)
    {
        if(i<=last-5)
        {
            data[i]=data[i+1];
        }else{
            data[i]='\0';
        }
    }
    

}


/* 3.解析字符串中的时间函数，转成时间戳 */ 
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



/* .解析传来的字符串 */ 
// char *resolve_str(char *data)
void resolve_str(char *data)
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
			**手环编号不存在
			**为手环初始化
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
			tags[now_tag].smo_li[0].dt = 0.0
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
			dt =(double) (tags[i].cur_t-tags[i].pri_t)/1000.0         
			if(!tags[i].is_smo){
				//不平稳
				if(dt>=TIME_LIM){
					//时间差大于10s,清空准平稳段列表重新获取
					tags[now_tag].smo_li[0].x = x;
					tags[now_tag].smo_li[0].y = y;
					tags[now_tag].smo_li[0].t = time;
					tags[now_tag].smo_li[0].dt = dt;
					tags[now_tag].smo_num = 1;
				}else{
					nn = tags[now_tag].smo_num;
					tags[now_tag].smo_li[nn].x = x;
					tags[now_tag].smo_li[nn].y = y;
					tags[now_tag].smo_li[nn].t = time;
					tags[now_tag].smo_li[nn].dt = dt;
					tags[now_tag].smo_num++;
					nn++;
					if(nn>=SMO_SIZE-1){
						//准平稳段已填满数据
						is_steady(i);
					}
				}
			}else{
				//平稳
				tags[now_tag].cur_x = x;
				tags[now_tag].cur_y = y;
				tags[now_tag].cur_t = time;
				strcpy(tags[i].pri_grid,tags[i].cur_grid);
				strcpy(tags[i].cur_grid,GeoHash(x,y,A));
				if(df<TIME_LIM){
					//相邻数据在时间差范围内
					// printf("Enter drift....\n");	
					confidentDegree = drift(i)*100;

				}else{
					confidentDgree = 0.0;
				}
			}
			cJSON_AddNumberToObject(json,"cd",confidentDegree);
			char *jsonStr = cJSON_Print(json);
			printf("置信度%.2f \n",confidentDegree);
			return jsonStr;	
		}else{
			return data;
		}
	}
}

/* 判断手环数据是否平稳 
** @param tag_pos int,手环在手环数组的下标 
*/
void is_steady(int tag_pos)
{
	int i;
	int count=0;
	double sp;
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
				break;
			}
		}
	}
}

double distf(double x1,double x2,double y1,double y2){
	return double dist = pow(pow(x1-x2,2.0)+pow(y1-y2,2.0),0.5);
}

double dists(smo s1,smo s2){
	return double dist = pow(pow(s1.x-s2.x,2.0)+pow(s1.y-s2.y,2.0),0.5);
}

double speed(double dist,double dt){
	return dist/dt;
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

// /* 判断漂移算法 
// ** @param tag_pos int,手环在手环数组的下标 
// ** return confidentDegree int,该条记录的置信度 
// */
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

// * 当前拥有的哈希节点数目 */ 
// int hash_table_size;
// HashNode* hashTable[HASH_MAX_SIZE];

// void Init_HashTable()
// {
// 	hash_table_size = 0;
// 	memset(hashTable,0,sizeof(HashNode *)*HASH_MAX_SIZE);
	
// 	printf("init over! \n");
// }

// unsigned int hash(const char *str)
// {
// 	const signed char *p = (const signed char*)str;
// 	unsigned int h = *p;
// 	if(h)
// 	{
// 		for(p+=1;*p!='\0';++p)
// 		{
// 			h = (h<<5)-h+*p;
// 		}
// 	}
	
// 	return h;
// }

// /* 哈希表插入 */ 
// void hash_insert(const char* key,char *accessKey,int accessTime)
// {
// 	if(hash_table_size == HASH_MAX_SIZE)
// 	{
// 		printf("out of memory! \n");
// 		return;
// 	}
	
// 	unsigned int pos = hash(key)%HASH_MAX_SIZE;
	
// 	HashNode *hashNode = hashTable[pos];
	
// 	int flag = 1;	/* 1为新，0为旧*/ 
// 	while(hashNode)
// 	{
// 		if(strcmp(hashNode->key,key)==0)
// 		{//已有节点 
// 			flag = 0;
// 			break;
// 		}
// 		hashNode = hashNode->next;
// 	}
	
// 	/* 创建新的节点 */ 
// 	if(flag)
// 	{
// 		printf("不存在键%s \n",key);
// 		HashNode *newNode = (HashNode *)malloc(sizeof(HashNode));
// 		memset(newNode,0,sizeof(HashNode));
// 		newNode->key = (char *)malloc(sizeof(char)*(strlen(key)+1));
// 		strcpy(newNode->key,key);
		
// 		Access *accessNode = (Access *)malloc(sizeof(Access));
// 		accessNode->key = (char *)malloc(sizeof(char)*(strlen(accessKey)+1));
// 		strcpy(accessNode->key,accessKey);
// 		accessNode->time = accessTime;
// 		accessNode->next = NULL;
// 		newNode->access = accessNode;
		
// 		newNode->next = hashTable[pos];
// 		hashTable[pos] = newNode;
// 		hash_table_size++;
// 	}
	
// 	/* 在原有节点上操作 */ 
// 	else
// 	{
// 		printf("已有键%s \n",key);
// 		Access *accessNode = (Access *)malloc(sizeof(Access));
// 		accessNode->key = (char *)malloc(sizeof(char)*(strlen(accessKey)+1));
// 		strcpy(accessNode->key,accessKey);
// 		accessNode->time = accessTime; 
// 		accessNode->next = NULL;
		
// 		/* 附加到原有的可达区域节点后面 */
// 		Access *p;
// 		p = hashNode->access;
// 		while(p->next)
// 		{
// 			p = p->next;	
// 		}
// 		p->next = accessNode;
// 	}
	
// 	printf("The size of HashTable is %d now! \n",hash_table_size);
// } 

// /* 哈希表搜索 */ 
// HashNode *hash_search(const char *key)
// {
// 	unsigned int pos = hash(key)%HASH_MAX_SIZE;
	
// 	if(hashTable[pos])
// 	{
// 		HashNode *pHead = hashTable[pos];
// 		while(pHead)
// 		{
// 			if(strcmp(key,pHead->key)==0)
// 				return pHead;
// 			pHead = pHead->next;
			
// 		}
// 	}
	
// 	return NULL;
// }


// /* 打印整个哈希表 测试用 */ 
// void display_hash_table()
// {
// 	for(int i=0;i<HASH_MAX_SIZE;i++)
// 	{
// 		if(hashTable[i])
// 		{
// 			HashNode *phead = hashTable[i];
// 			while(phead)
// 			{
// 				printf("%s的可达区域是：\n",phead->key);
				
// 				Access *as = phead->access;
// 				while(as)
// 				{
// 					printf("区域编号：%s;时间:%d \n",as->key,as->time);
// 					as = as->next;
// 				}
				
// 				phead = phead->next;
// 			}
// 		}
// 	}	
// }

