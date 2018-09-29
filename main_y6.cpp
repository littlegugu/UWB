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

#define HASH_MAX_SIZE 50000 /* ��ϣ�ڵ����ֵ */  
#define EVEN(x) ((x % 2) == 0)
#define TAG_SIZE 100 /* �ֻ��� */ 
#define X_MIN 0		/* ƽ��ͼx��Сֵ */ 
#define X_MAX 100	/* ƽ��ͼx���ֵ */ 
#define Y_MIN 0		/* ƽ��ͼy��Сֵ */ 
#define Y_MAX 100	 /* ƽ��ͼy���ֵ */ 
#define SMO_MAX_NUM 10	/* ׼ƽ������ */ 
#define SP_LIM 5	// �ٶ�	 
#define A 20 	//geoHash���ִ��� 
#define same_lim 3//�ַ���ǰnλ��ͬ���� 
#define LINE 30     /*txt�ļ�����󳤶�*/
#define ROOM_QUANT 3 /*������������*/
#define WALL_QUANT 4 /*������ǽ������*/
#define COR_X 0.69/*����x*/
#define COR_Y 0.94/*����y*/
#define GEO_BIN_LEN 8/*Geohash�������ַ���*/
#define GEO_STR_LEN 4/*Geohash��ĸ�ַ�������*/
#define BASE32_LAY_LEN 5 /*Geohash�ַ�������󳤶�*/
#define BASE32_MIN_LEN 8 /*BASE32ÿ��������ַ����ȣ����ٸ���������Ϊһ���ַ�*/
#define GRID_QUANT 60000 /*դ������*/
#define STEP 30 /*����*/

static const char base32_alphabet[32] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'j', 'k', 'm', 'n', 'p', 'q', 'r',
        's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

/* �ɴ����� */ 
typedef struct Access{	
	char *key;
	int time=0;
	Access *next;
};

/* ��ϣ�ڵ� */ 
typedef struct HashNode{
	char *key;
	/* �ɴ��������� �����ǽ�1��3��5��Ķ�д�����Ҳ���Կ��������� */
	Access *access;		/* �ɴ�����������׵�ַ */ 
	Access *accessRear;	/* �ɴ�������������һ�� */ 
	HashNode *next;	/* ��һ���ڵ� */ 
};

/*׼ƽ�ȶ�*/
typedef struct smo{
	double x;//��ǰ
	double y;
	long int t;
};

/*�ֻ�������*/
typedef struct Tag{
	char *id;	/* �ֻ���� */ 
	double cur_x;	//��ǰx 
	double cur_y;	// ��ǰy 
	long int cur_t;	//��ǰ����ʱ�� 
	char *cur_grid;	//��ǰ������ 
	double pri_x;	//��һx 
	double pri_y;	//��һy 
	long int pri_t;	//��һ�ε�ʱ�� 
	char *pri_grid;	//��һ�ε������� 
	int is_smo=0;	//�Ƿ�ƽ�ȣ� 1Ϊƽ�ȣ�0Ϊ��ƽ�� 
	smo smo_li[10];		//10��׼ƽ������ 
	int smo_num=0;	//�Ѵ��ƽ������,��Ϊ0���ʾ���ֻ�û��ʼ���� 
};

typedef struct wall{
    double top_x;
	double low_x;
	double top_y;
	double low_y;
};

typedef struct room{
    int room_num;/*�����������*/
	int door_pos;/*�ŵ�λ��*/
	int room_type;/*�������ͣ�0��׼��������1�Ǳ�׼��������2����ͨ�з�������*/
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
	int grid_num;/*������դ������*/
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
	int areaCount;/*�ڷ��������ڵڼ���դ��*/
    /*�����±���Ϊ���������ڵı��*/
};

// typedef struct areaGrid{
//     struct grid areaGridArr[GRID_QUANT];
//     int index = 0;//��ʼ���󳤶�
// }areaGrid;

typedef struct Elem{
	char *elem;
};

/* �̳߳� */ 
typedef struct threadpool_t{
	pthread_mutex_t lock;	/* ������ */ 
	pthread_cond_t queue_not_empty;		/* ������в�Ϊ�� */
	pthread_cond_t queue_not_full;	/* ������в��� */

	pthread_t *threads;	/* �����߳����� */
	pthread_t rec_tid;	/* �����߳� */

	SOCKET send_udp;	/* udp�ͻ��� */ 
	sockaddr_in sin;
	int sin_len;

	Elem *queue;		/* �������� */
	int queue_front;	/* ��ͷ */
	int queue_rear;		/* ��β */
	unsigned int queue_size;		/* ��ǰ���ݶ��д�С */ 

};

/* ȫ�ֱ��� */ 
int QUEUE_MAX_SIZE; 
int RECEIVE_PORT;	//���ն˿� 
int SEND_PORT; 	//���Ͷ˿� 
char *SEND_ADDR;	//���͵�ַ
int THREAD_NUM;	//�����߳��� 
/* ȫ�ֱ��� */ 
/* �ֻ����� */ 
Tag tags[TAG_SIZE];
/* ��ǰӵ�еĹ�ϣ�ڵ���Ŀ */ 
int hash_table_size;
HashNode* hashTable[HASH_MAX_SIZE];

/* access_lock */
pthread_mutex_t access_lock;

/* ���ش����Ŀͻ���tcp_socket */
static SOCKET tcp_client;
/* ���ڴ洢�������Ļ�����Ϣ */
static struct sockaddr_in tcp_server_in;
struct room rooms[ROOM_QUANT];/*������������*/
struct grid grids[GRID_QUANT];/*դ������*/
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

    tcp_server_in.sin_family = AF_INET;    //IPV4Э����
    tcp_server_in.sin_port = htons(13107);  //�������Ķ˿ں�
    tcp_server_in.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //����IP

    while(connect(tcp_client, (struct sockaddr *)&tcp_server_in, sizeof(tcp_server_in)) == SOCKET_ERROR)
    {
        printf("connection failed��reconnection after 3 seconds��\n");
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

/* ��ϣ����� */ 
void hash_insert(const char* key,char *accessKey,int accessTime)
{
	if(hash_table_size == HASH_MAX_SIZE)
	{
		printf("out of memory! \n");
		return;
	}
	
	unsigned int pos = hash(key)%HASH_MAX_SIZE;
	
	HashNode *hashNode = hashTable[pos];
	
	int flag = 1;	/* 1Ϊ�£�0Ϊ��*/ 
	while(hashNode)
	{
		if(strcmp(hashNode->key,key)==0)
		{//���нڵ� 
			flag = 0;
			break;
		}
		hashNode = hashNode->next;
	}
	
	/* �����µĽڵ� */ 
	if(flag)
	{
		printf("�����ڼ�%s \n",key);
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
	
	/* ��ԭ�нڵ��ϲ��� */ 
	else
	{
		printf("���м�%s \n",key);
		Access *accessNode = (Access *)malloc(sizeof(Access));
		accessNode->key = (char *)malloc(sizeof(char)*(strlen(accessKey)+1));
		strcpy(accessNode->key,accessKey);
		accessNode->time = accessTime; 
		accessNode->next = NULL;
		
		/* ���ӵ�ԭ�еĿɴ�����ڵ���� */
		Access *p;
		p = hashNode->accessRear;
		p->next = accessNode;
		hashNode->accessRear = accessNode;
	}
	
	printf("The size of HashTable is %d now! \n",hash_table_size);
} 

/* ��ϣ������ */ 
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


/* ��ӡ������ϣ�� ������ */ 
void display_hash_table()
{
	for(int i=0;i<HASH_MAX_SIZE;i++)
	{
		if(hashTable[i])
		{
			HashNode *phead = hashTable[i];
			while(phead)
			{
				printf("%s�Ŀɴ������ǣ�\n",phead->key);
				
				Access *as = phead->access;
				while(as)
				{
					printf("�����ţ�%s;ʱ��:%d \n",as->key,as->time);
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


// /* geohash���� 
// **	@param x double,����x
// **	@param y double,����y
// **  @param a int,���� 
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
        
//         /* ��߱���0���ұ߱���1 */ 
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

//     //������������кϲ������Ϊһ�������Ʊ���
//     /* ��λ��y��żλ��x */
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


/* ���ٶ� */ 
float  speed(smo s1,smo s2)
{
	float diff_time = (s2.t-s1.t)/1000.0;
	float dist = pow(pow(s1.x-s2.x,2.0)+pow(s1.y-s2.y,2.0),0.5);
	float sp = dist/diff_time;
	return sp;
}

/* �ж��ֻ������Ƿ�ƽ�� 
** @param tag_pos int,�ֻ����ֻ�������±� 
*/
void is_steady(int tag_pos)
{
	printf("it's is_steady! \n");
	/* ����ж��Ѿ�ƽ�����޸�tags[i].is_smo=1 */ 
	if(tags[tag_pos].smo_num<10)
	{//��������10������ 
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
				{	/* ����7��ƽ�ȼ�¼�����ж�ƽ�� �޸�ƽ���ֶ�*/ 
					tags[tag_pos].is_smo=1;
					printf("�ֻ����ȶ�...\n");
					break;
				}
			}
		}
	}
	
}

/* �ж�Ư���㷨 
** @param tag_pos int,�ֻ����ֻ�������±� 
** return confidentDegree int,������¼�����Ŷ� 
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
	
// 	/* ǰnλһ�����ò�������ڽ����� */ 
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
// 			printf("�����ֻ����������ڿɴ����.... \n");
// 		}
		
// 	}
	
// 	return 0.0;
// }  

/* �����������ַ��� */ 
char *resolve_str(char *data)
{
	/* ��cJSON���� */ 
	cJSON *json = cJSON_Parse(data);
	if(json)	//�����ɹ� 
	{
		/* �ж��Ƿ��ѳ�ʼ�����ֻ���0Ϊ�ޣ�1Ϊ�� */ 
		int flag = 0;
		/* �ֻ����ֻ���������±� */ 
		int i;	
		/* �������� */ 
		char *id = cJSON_GetObjectItem(json, "tagid")->valuestring;	/* id */ 
		double x = cJSON_GetObjectItem(json, "x")->valuedouble;	/* ����x */ 
		double y = cJSON_GetObjectItem(json, "y")->valuedouble;	/* ����y */ 
		char *time_str = cJSON_GetObjectItem(json, "time")->valuestring;	/* ʱ�� */ 
		long int time = atol(time_str);
		
		/* ���ֻ�����Ѱ���ֻ� */ 
		for(i=0;i<TAG_SIZE;i++)
		{
			if(tags[i].smo_num == 0)
				break;
			
			if(strcmp(tags[i].id,id)==0)
			{
				//������������ 
				flag = 1;
				break;		
			} 
		}
		
		printf("���ֻ���������±��ǣ�%d \n",i);
		
		/* �����ڵ�ʱ�򣬽��г�ʼ�� */ 
		// if(!flag)
		// {
		// 	/* Ϊid���ٿռ� */
		// 	printf("-------����Ϊ�ֻ�%s��ʼ��------- \n",id);
		// 	tags[i].id = (char *)malloc(strlen(id));
		// 	strcpy(tags[i].id,id);
			
		// 	tags[i].cur_x = tags[i].pri_x = x;
		// 	tags[i].cur_y = tags[i].pri_y = y;
		// 	tags[i].cur_t = tags[i].pri_t = time;
		// 	tags[i].smo_li[0].x = x;
		// 	tags[i].smo_li[0].y = y;
		// 	tags[i].smo_num = 1;
			 
		// 	/* �õ�������룬ʹ��geohash */
		// 	// char *geoHash = GeoHash(x,y,A);
		// 	tags[i].cur_grid = (char *)malloc(strlen(geoHash)+1);
		// 	tags[i].pri_grid = (char *)malloc(strlen(geoHash)+1);
		// 	strcpy(tags[i].cur_grid,geoHash);
		// 	strcpy(tags[i].pri_grid,geoHash);
			
		// 	printf("��ʼ��%s ,����������%s \n",tags[i].id,tags[i].cur_grid); 
		// }
		// else
		// {/* ����ֻ��Ѿ����������� */
		// 	printf("���ֻ��Ѿ����ڣ� \n");
		// 	/* 
		// 	** ���ж��ֻ��ǲ����Ѿ�ƽ�ȣ����ֶ�is_smo�Ƿ����1�� 
		// 	** ����ֻ�û��ƽ�ȣ�������ж��Ƿ�ƽ�ȵĺ���
		// 	** ����ֻ��Ѿ�ƽ�ȣ�������ж��Ƿ�Ư�Ƶĺ��� 
		// 	*/ 
			
		// 	/* �޸����� ����һ����¼�ĳɵ�ǰ������ǰ������Ϊ��ȡ������*/
		// 	tags[i].pri_x = tags[i].cur_x;
		// 	tags[i].pri_y = tags[i].cur_y;
		// 	tags[i].pri_t = tags[i].cur_t;
		// 	strcpy(tags[i].pri_grid,tags[i].cur_grid);
			
		// 	tags[i].cur_x = x;	tags[i].cur_y = y;
		// 	tags[i].cur_t = time;	
		// 	strcpy(tags[i].cur_grid,GeoHash(x,y,A));
		// 	printf("�ֻ�%s��ǰλ��Ϊ%s \n",tags[i].id,tags[i].cur_grid);
			
		// 	/* ����ֻ���ƽ�� */
		// 	if(tags[i].is_smo==0)
		// 	{
		// 		/* �ȴ���������� */ 
		// 		/* �������׼ƽ�ȶ���������10 ����ֱ����������*/
		// 		if(tags[i].smo_num<SMO_MAX_NUM)
		// 		{
		// 			tags[i].smo_li[tags[i].smo_num].x = x;
		// 			tags[i].smo_li[tags[i].smo_num].y = y;	
		// 			tags[i].smo_li[tags[i].smo_num].t = time;
		// 			tags[i].smo_num++;
		// 		}
		// 		else
		// 		{/*����Ѿ���10���� ��յ�1�� �����θ������ݵ����һ��*/ 
		// 			for(int j=1;j<SMO_MAX_NUM;j++)
		// 			{//ǰŲһ����¼ 
		// 				tags[i].smo_li[j-1].x = tags[i].smo_li[j].x;
		// 				tags[i].smo_li[j-1].y = tags[i].smo_li[j].y;
		// 				tags[i].smo_li[j-1].t = tags[i].smo_li[j].t;
		// 			}
		// 			tags[i].smo_li[SMO_MAX_NUM-1].x = x;
		// 			tags[i].smo_li[SMO_MAX_NUM-1].y = y;
		// 			tags[i].smo_li[SMO_MAX_NUM-1].t = time;
		// 		}
				
		// 		/* �����ж��Ƿ�ƽ�Ⱥ��� ֻ��Ҫ���ֻ��±괫��ȥ*/
		// 		is_steady(i);
		// 		printf("׼ƽ����������:%d \n",tags[i].smo_num);	
		// 	}
		// 	else
		// 	{/* ����ֻ��Ѿ�ƽ�Ƚ��뵽Ư���㷨 */ 
		// 		printf("Enter drift....\n");
		// 		float confidentDegree = drift(i);
		// 		cJSON_AddNumberToObject(json,"cd",confidentDegree);
		// 		char *jsonStr = cJSON_Print(json);
				
		// 		printf("���Ŷ�%.2f \n",confidentDegree);
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
// 	/* ��¼����ʱ�� */ 
// 	clock_t start,finish;
// 	double total_time;
	
// 	char *buf,*p;
// 	char *filename = (char *)arg;
	
// 	printf("filename: %s \n",filename);
	
// 	FILE *fp = fopen(filename, "r+");
// 	if(fp==NULL)
// 	{
// 		printf("open file fail��\n");
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
// 		p = ReadData(fp,buf);//ÿ�ε����ļ�ָ��fp���Զ�����һ��
// 		if(!p)
// 		{
// 			break;	//ÿ�ε����ļ�ָ��fp���Զ�����һ��
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

/*�ָ�txt�ַ���*/
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
	FILE * fp = NULL;/*�ļ�ָ��*/
    char * buf;/*���ݻ��������ǵ�free*/
	buf = (char *)malloc(LINE*sizeof(char));
	fp = fopen(filename,"r+");/*�ָ������ļ�*/
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

/*���ַ���̬��Geohash����*/
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
	FILE * fp = NULL;/*�ļ�ָ��*/
    char * buf;/*���ݻ��������ǵ�free*/
	buf = (char *)malloc(LINE*sizeof(char));
	fp = fopen(filename,"r+");/*�������������ļ�*/
	double buflist[5];
	double x_range[]={0,0};/*���ǽ��,��С�ſ�;0:�½磻1:�Ͻ�*/
    double y_range[]={0,0};
	double wx,wy,dx,dy;
	int count=0,rN = 0;/*count�����ݵ��ڼ��У�rN������ţ�*/
	if (fp) {
		printf("File %s reading...\n",filename);/* file exist. */
		while (fgets(buf,LINE,fp)!=NULL){
			// puts(buf);
			if (strlen(buf)<3){
				if (count=4){
					rooms[rN].room_type=0;/*��׼����*/
				}else{
					rooms[rN].room_type=1;/*������*/
				}
				count = 0;
				rN ++;
			}else{
				split(buf,buflist);
				if (count==0) {
					rooms[rN].room_num   = rN;/* �� */
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
					/* ǽ */
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
	//��̬���㾫�ȷ�Χ
    if(x_range[1]>y_range[1]){
        alpha      = dichotomy(x_range,areas.top_x,areas.low_x);//ͨ��x��Χ������ִ���
    }else{
        alpha      = dichotomy(y_range,areas.top_y,areas.low_y);//ͨ��y��Χ������ִ���
    }
	printf("%d\n",alpha);
	return 0;
}

/*��λ*/
char* complement(char * str,int digit)
{
	int st = strlen(str);/*�ַ�������*/
	if(st<digit)
	{
		/*����digitλ��λ*/
		int diff = digit-st;
		char *tmp;
		tmp = (char *)malloc(digit*sizeof(char));
		strcpy(tmp,str);
		for(int i = 0; i < digit; i++)
		{
			if (i<diff)
				str[i] = '0';/* ��0 */
			else
				str[i] = tmp[i-diff];/* ��λ */
		}
		str[digit]='\0';
		free(tmp);
		tmp = NULL;
	}

	return str;
}

/*ʮ����ת������*/
char* DectoBin(char* str, int count,int alpha)
{
    itoa(count, str, 2);/*��תʮ*/
    complement(str,alpha);/*����alphaλ����λ*/
    return str;
}

/*��Geohash���ַ�����դ�񣬷��ض������ַ���*/
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
            str[i]=xStr[j];/* żx */
        else 
            str[i]=yStr[j];/* �� */
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
	complement(bin_source,BASE32_MIN_LEN);//����8λ��λ
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

/*���ҷ������*/
int GetRoomNum(double x,double y)
{
    for (int rN = 0; rN < ROOM_QUANT; rN++)
    {
        if ((rooms[rN].low_x<=x+xSize && rooms[rN].top_x>x) && (rooms[rN].low_y<=y+ySize && rooms[rN].top_y>y))
            return rN;
    }
    return -1;
}

/*�鿴դ��ͨ�����*/
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
                /*դ����ǽ�н���*/
                if(x+(xSize/2)-rooms[rN].walls[wN].top_x<0)
                    cant[0] = 1;/*0���򣬲�ȡ����*/
                if(y+(ySize/2)-rooms[rN].walls[wN].top_y<=0)
                    cant[1] = 1;/*1����ȡ����*/
                if(rooms[rN].walls[wN].low_x-(x+(xSize/2))<=0)
                    cant[2] = 1;/*2����ȡ����*/
                if(rooms[rN].walls[wN].low_y-(y+(ySize/2))<0)
                    cant[3] = 1;/*3���򣬲�ȡ����*/            
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


/*ʹ��geohashȫͼ�ָ�դ��*/
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

/* ��ʼ�� ����ɴ����� */ 
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

/*��ֵ*/
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


/*  ����x,y���꣬����������ַ���
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
                strBin[i*2] = '0';/* ��0 */
                x_max = x_mid;
            }
            else {
                strBin[i*2] = '1';/* ��1 */
                x_min = x_mid;
            }
            if (y<=y_mid) {
                strBin[i*2+1] = '0';/* ��0 */
                y_max = y_mid;
            }
            else {
                strBin[i*2+1] = '1';/* ��1 */
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
        assignment(i,i,1,mat,all);//�ԽǱ�
		assignment(j,j,1,mat,all);
        if (grids[i].east == 0) {
            p = j + 1;/* �� */
			// k = rooms[rN].grid_list[p];
            if(p >= 0 && p < all) {
				i = rooms[rN].grid_list[p];
				i = rooms[rN].grid_list[p];
				if (grids[i].west == 0){
					assignment(j,p,1,mat,all);
					if(grids[i].north == 0){
						p = j + pow(2,alpha);/* �� */
						// k = rooms[rN].grid_list[p];
						if(p >= 0 && p < all) {
							i = rooms[rN].grid_list[p];
							if (grids[i].south == 0){
								assignment(i,p,1,mat,all);
								p = j + pow(2,alpha) + 1;/* ���� */
								// k = rooms[rN].grid_list[p];
								assignment(i,p,1,mat,all);
								if (grids[i].west == 0) {
									p = j - 1;/* �� */
									// k = rooms[rN].grid_list[p];
									if(p >= 0 && p < all) {
										i = rooms[rN].grid_list[p];
										if (grids[i].east == 0){
											assignment(i,p,1,mat,all);
											p = j + pow(2,alpha) - 1;/* ���� */
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
                    p = j - pow(2,alpha);/* �� */
					// k = rooms[rN].grid_list[p];
                    if(p >= 0 && p < all) {
						i = rooms[rN].grid_list[p];
						if (grids[i].north == 0){
							assignment(i,p,1,mat,all);
							p = j - pow(2,alpha) + 1;/* ���� */
							// k = rooms[rN].grid_list[p];
							assignment(i,p,1,mat,all);
							if (grids[i].west == 0) {
								p = j - 1;/* �� */
								// k = rooms[rN].grid_list[p];
								if(p >= 0 && p < all) {
									i = rooms[rN].grid_list[p];
									if (grids[i].east == 0){
										assignment(i,p,1,mat,all);
										p = j + pow(2,alpha) - 1;/* ���� */
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
            p = j - 1;/* �� */
			// k = rooms[rN].grid_list[p];
            if(p >= 0 && p < all) {
				i = rooms[rN].grid_list[p];
				if (grids[i].east == 0){
						assignment(i,p,1,mat,all);
						if(grids[i].north == 0){
							p = j + pow(2,alpha);/* �� */
							// k = rooms[rN].grid_list[p];
							if(p >= 0 && p < all) {
								i = rooms[rN].grid_list[p];
								if (grids[i].south == 0){
									assignment(i,p,1,mat,all);
									p = j + pow(2,alpha) - 1;/* ���� */
									// k = rooms[rN].grid_list[p];
									assignment(i,p,1,mat,all);
								}
							}
						}
					}
                }
                if(grids[i].south == 0){
                    p = j - pow(2,alpha);/* �� */
					// k = rooms[rN].grid_list[p];
                    if(p >= 0 && p < all) {
						i = rooms[rN].grid_list[p];
						if (grids[i].north == 0){
							assignment(i,p,1,mat,all);
							p = j - pow(2,alpha) - 1;/* ���� */
							// k = rooms[rN].grid_list[p];
							assignment(i,p,1,mat,all);
						}
                    }
                }                                
            
        }else{
            if(grids[i].north == 0){
                p = j + pow(2,alpha);/* �� */
				// k = rooms[rN].grid_list[p];
                if(p >= 0 && p < all) {
					i = rooms[rN].grid_list[p];
					if (grids[i].south == 0)
                    	assignment(i,p,1,mat,all);
				}
            }
            if(grids[i].south == 0){
                p = j - pow(2,alpha);/* �� */
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
        // double * ap = (double *)a;//��ȡ����
        // double * bp = (double *)b;//��ȡ����a���׵�ַ 
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

/* ��������  */
void *func_process(void *thr_pool)
{
	threadpool_t *pool = (threadpool_t *)thr_pool;
	
	/* ��ʼ��udp�ͻ��� */
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

	/* ��¼����ʱ�� */ 
	clock_t start,finish;
	double total_time;
	
    
	while(1)
	{	
		/* ���� */
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

		
		/* ֪ͨ������������� */
		pthread_cond_broadcast(&(pool->queue_not_full));
		
		/* �����ȡ�������� */ 
		  
		char *result = resolve_str(a_data);
		
		if(result!=NULL)
			sendto(udpclient,result, strlen(result), 0, (sockaddr *)&sin, len);	//����udp�� 
		
		finish = clock();
		total_time = (double)(finish-start)/CLOCKS_PER_SEC;
		
		printf("Runnig time of this process:%0.4f ms \n",total_time);
		
		/* ���� */ 
		pthread_mutex_unlock(&(pool->lock));	
	}
	
}

//���պ��� 
void *func_receive(void *thr_pool)
{
	threadpool_t *pool = (threadpool_t *)thr_pool;
	
	while(1)
	{	
		pthread_mutex_lock(&(pool->lock));
		
		/* ����������ˣ�����wait���� */  
		while(pool->queue_size == QUEUE_MAX_SIZE)
		{
			printf("queue full!wating! \n"); 
			pthread_cond_wait(&(pool->queue_not_full),&(pool->lock));
		}
		
		char recvData[255];  	/* ���������������� ���255*/  
		int ret = recv(tcp_client,recvData,255,0);
		if (ret > 0)
        {
          	recvData[ret] = '\0';
            pool->queue[pool->queue_rear].elem = recvData;
            pool->queue_rear = (pool->queue_rear+1)%QUEUE_MAX_SIZE;	/* ��״���� */ 
            pool->queue_size++;
            printf("%s \n",recvData);
            printf("Receive data,queue_size is %d now!\n",pool->queue_size);
            
            /* ��������ݺ󣬶��в�Ϊ�գ�����һ�������߳� */
			pthread_cond_signal(&(pool->queue_not_empty)); 
    	}
		
		else if(ret == 0)
        {
            //��ret == 0 ˵�����������ߡ�
            printf("Lost connection , Ip = %s\n", inet_ntoa(tcp_server_in.sin_addr));
            closesocket(tcp_client);
            reconnect();//����
        }
        else
        {
        	//��ret < 0 ˵���������쳣 ��������״̬��������߶�ȡ����ʱ����ָ�����ȡ�
            //������������Ҫ�����Ͽ��Ϳͻ��˵����ӡ�
            printf("Something wrong of %s\n", inet_ntoa(tcp_server_in.sin_addr));
            closesocket(tcp_client);
            reconnect();//����
        }	
	
		pthread_mutex_unlock(&(pool->lock));	
	}
	
}


/* ��ʼ�� */ 
threadpool_t *init(int thr_num)
{
	int i;
	threadpool_t *pool = NULL;
	
	/* ��ʼ��tcp�ͻ��� */ 
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
		/* ��ʼ���̳߳� */
		if((pool=(threadpool_t *)malloc(sizeof(threadpool_t)))==NULL)
		{
			printf("malloc threadpool fail! \n");
			break;
		}
		
		/* ��ʼ������������������ */
		if ( pthread_mutex_init(&(pool->lock), NULL) != 0 ||
       		pthread_cond_init(&(pool->queue_not_empty), NULL) !=0  ||
       		pthread_cond_init(&(pool->queue_not_full), NULL) !=0)
      	{
        	printf("init lock or cond false;\n");
        	break;
      	}
		  
		/* ���п��ռ� */	
      	pool->queue = (Elem *)malloc(sizeof(Elem)*QUEUE_MAX_SIZE);
      	if(pool->queue == NULL)
      	{
      		printf("malloc queue fail! \n");
      		break;
		}
      	/* ���г�ʼ�� */
		pool->queue_front = 0;
		pool->queue_rear = 0;   
      	pool->queue_size = 0;
      	
      	/* �������߳̿��ռ� */
		pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*thr_num);
		if(pool->threads == NULL)
		{
			printf("malloc threads fail! \n");
			break;
		} 
		memset(pool->threads,0,sizeof(pthread_t)*thr_num);
		/* ���������߳� */
		for(i=0;i<thr_num;i++)
		{
			pthread_create(&(pool->threads[i]),NULL,func_process,(void *)pool);
			printf("start thread 0x%x....\n",(unsigned int)pool->threads[i]);	
		} 
		
		/* ���������߳� */
		pthread_create(&(pool->rec_tid),NULL,func_receive,(void *)pool);
		
		return pool; 
	}while(0);
	
	return NULL;
}

int readConfiguration(char *fileAddr)
{
	printf("------���ڶ�ȡ�����ļ�%s--------\n",fileAddr);
	
	/* ����cJSON���� */ 
	cJSON *json = NULL;	 
	
	/* �Զ���ʽ������json�ļ� */
	FILE *fp = fopen(fileAddr,"rb");
	if(fp == NULL)
	{
		printf("Open file fail! \n");
		return 0;
	}
	
	fseek(fp,0,SEEK_END);
	int len = ftell(fp);
	fseek(fp,0,SEEK_SET);
	
	char *jsonStr = (char *)malloc(sizeof(char)*(len+1)); /* ����ռ� */ 
	fread(jsonStr,1,len,fp);	/* ��ȡ�����ļ�ΪjsonStr�ַ��� */ 
	fclose(fp);	/* �ر��ļ� */ 
	
	json = cJSON_Parse(jsonStr);	/* ת��Ϊjson���� */ 
	if(json == NULL)
	{
		printf("Read configuration fail! \n");
		return 0;
	}
	
	/* ��ȡ����������� */ 
	cJSON *sub = cJSON_GetObjectItem(json,"QUEUE_MAX_SIZE");	/* ������� */
	if(sub == NULL)
	{
		printf("Read QUEUE_MAX_SIZE fail! \n");
		return 0;	
	}
	QUEUE_MAX_SIZE = sub->valueint;
	printf("QUEUE_MAX_SIZE:%d \n",QUEUE_MAX_SIZE);
	
	/* ��ȡ���ն˿� */ 
	sub = cJSON_GetObjectItem(json,"RECEIVE_PORT");	/* ���ն˿� */
	if(sub == NULL)
	{
		printf("Read RECEIVE_PORT fail! \n");
		return 0;	
	}
	RECEIVE_PORT = sub->valueint;
	printf("RECEIVE_PORT:%d \n",RECEIVE_PORT);
	
	/* ��ȡ���Ͷ˿� */ 
	sub = cJSON_GetObjectItem(json,"SEND_PORT");	/* ���Ͷ˿� */
	if(sub == NULL)
	{
		printf("Read SEND_PORT fail! \n");
		return 0;	
	}
	SEND_PORT = sub->valueint;
	printf("SEND_PORT:%d \n",SEND_PORT);
	
	/* ��ȡ���͵�ַ */
	sub = cJSON_GetObjectItem(json,"SEND_ADDR");	/* ���͵�ַ */
	if(sub == NULL)
	{
		printf("Read SEND_ADDR fail! \n");
		return 0;	
	}
	SEND_ADDR = sub->valuestring;
	printf("SEND_ADDR:%s \n",SEND_ADDR); 
	
	/* ��ȡ�����߳��� */ 
	sub = cJSON_GetObjectItem(json,"THREAD_NUM");	/* �����߳��� */
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
	
// 	/* ��ʼ���̳߳أ�����THREAD_NUM�������߳� */ 
// 	threadpool_t *pool = init(THREAD_NUM);	
// 	/* ֱ�������߳�ֹͣ��������ֹ */ 
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


