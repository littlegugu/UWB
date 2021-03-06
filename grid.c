#include <stdio.h>
#include <stdlib.h>

#include <winsock2.h>
#include <unistd.h>	
#include <string.h>
#include <time.h>
#include <math.h>
#include <windef.h>
#include <windows.h>
#include <malloc.h>

#define LINE 100
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
#define HASH_MAX_SIZE 10000


/* 可达区域 */ 
typedef struct Access{	
	char *key;
	int time=0;
	struct Access *next;
};

/* 哈希节点 */ 
typedef struct HashNode{
	char *key;/* 可达区域数组 这里是将1、3、5秒的都写在这里，也可以开三个数组 */
	struct Access *access;	
	struct HashNode *next;	/* 下一个节点 */ 
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
}areaGrid;

struct room rooms[ROOMNUM];//声明两个房间
struct area areas;
areaGrid grids[ROOMQUANTITY];
// double area[4];
double door_x = 0.0;
double door_y = 0.0;
double wall_x = 0.0;
double wall_y = 0.0;
int alpha=0;//二分次数
/* 当前拥有的哈希节点数目 */ 
int hash_table_size;
HashNode* hashTable[HASH_MAX_SIZE];

static const char base32_alphabet[32] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'j', 'k', 'm', 'n', 'p', 'q', 'r',
        's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

void split(char * data,double *list);
// double * dot(double * a_mat, double * b_mat,int a_row,int b_row);
// double * mat_pow(double * a_mat,int a_row,int power);
// void init(void);
double max_f(double max_p,double a_num);
double min_f(double min_p,double a_num);
int * geohash_grid(void);
int dichotomy(double * range,double top,double low);
int init(void);
char* binGeohash(char* str, int count,int alpha);
char* geohash(char *str,int xCount,int yCount,int alpha);
char* base32_encode(char *bin_source,char * code);
int binToDec(char * binStr);
char* complement(char * str,int digit);
int getRoomNum(double x,double y);
<<<<<<< HEAD
int inWall(double x,double y,double xSize,double ySize,int rN);
int getRoomNum(double x,double y,double xSize,double ySize);
int assignment(int row,int col,int value,int * mat,int all);
int * connectedMatrix(int rN);
int canGet(void);
int * dot(int * a_mat, int * b_mat,int a_row,int b_row,int a_col,int b_col);
void Init_HashTable(void);
unsigned int hash(const char *str);
void hash_insert(const char* key,char *accessKey,int accessTime);
HashNode *hash_search(const char *key);
int main(int argc, char const *argv[])
{
    init();    
    int *  areaGridDec = geohash_grid();	
	Init_HashTable();
    time_t t;
	t = time(NULL);
	int i1 = time(&t);//计时器
    canGet();   
    int i2 = time(&t);//计时器
    printf("Make a can_get hashtable in %d s.\n",i2-i1);
    // canGet();
    // for(int i = 0; i <grids[1].index; i++)
    // {
    //     printf("num=%d,(x=%f,y=%f):%d,%d,%d,%d\n",i,grids[1].areaGridArr[i].x,grids[1].areaGridArr[i].y,grids[1].areaGridArr[i].east,grids[1].areaGridArr[i].west,grids[1].areaGridArr[i].south,grids[1].areaGridArr[i].north);
    // }

    // printf("%d\n",areaGridDec[0]);
=======
int* inWall(double x,double y,double xSize,double ySize,int rN,int *posList);
int main(int argc, char const *argv[])
{

    init();
    puts("ppp1");
    // for(int i = 0; i < ROOMNUM; i++)//打印room坐标
    //     printf("room%d,xtop=%f,xlow=%f,ytop=%f,ylow=%f\n",i,rooms[i].top_x,rooms[i].low_x,rooms[i].top_y,rooms[i].low_y);
    
    int *  areaGridDec = geohash_grid();
    // time_t t;
	// t = time(NULL);
	// int ii = time(&t);//计时器
    printf("%d\n",areaGridDec[0]);
>>>>>>> cb3b8b2b6385b394a968a1e7cded5700010d6b7c
    // int jj = time(&t);//计时器
    // printf("time:%d\n",jj-ii);
    // printf("areaGridDec:%d byte",_msize(areaGridDec)); //变量内存

    getchar();
    return 0;
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
<<<<<<< HEAD
            if (rN>=0) {
                // puts("inroom!\n");
                grids[rN].areaGridArr[grids[rN].index].x = x;
                grids[rN].areaGridArr[grids[rN].index].y = y;
                grids[rN].areaGridArr[grids[rN].index].numDec = index;
                strcpy(grids[rN].areaGridArr[grids[rN].index].numStr,geostr);
                inWall(x,y,xSize,ySize,rN);
            }
=======
            int p[8];
            inWall(x,y,xSize,ySize,rN,p);
            printf("%d\n",sizeof(p)/sizeof(int));
            printf("index:%d,",index);
            for (int i = 0; i < sizeof(p)/sizeof(int); i++)
            {
                printf("%d,",p[i]);
            }
            printf("\n");
            // printf("%d\n",index);
>>>>>>> cb3b8b2b6385b394a968a1e7cded5700010d6b7c
            // fprintf(fp,"x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,binToDec(binStr));
            // printf("x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,index);
        }
        
    }
    // fclose(fp);
    return area_num;
}

<<<<<<< HEAD

int inWall(double x,double y,double xSize,double ySize,int rN)
=======
int* inWall(double x,double y,double xSize,double ySize,int rN,int *posList)
>>>>>>> cb3b8b2b6385b394a968a1e7cded5700010d6b7c
{
    if(rN >= 0)
    {
        int cant[] = {0,0,0,0,0};
        for(int wN = 0; wN < WALLNUM; wN++)
<<<<<<< HEAD
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

=======
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
        int j = 1;
        int pos = 4;
        int count = 0;
        for (int i = 0; i < 4; i++)
        {
            if (i==3)
                j = 0;
            if (cant[i]==1)
            {
                posList[count++] = i;
                if(cant[j]==1)
                    posList[count++] = pos;
            }
            pos++;
        }
        return posList;
    }
    return NULL;
}


>>>>>>> cb3b8b2b6385b394a968a1e7cded5700010d6b7c




int getRoomNum(double x,double y,double xSize,double ySize)
{
    for (int rN = 0; rN < ROOMNUM; rN++)
    {
        if ((rooms[rN].low_x<=x+xSize && rooms[rN].top_x>x) && (rooms[rN].low_y<=y+ySize && rooms[rN].top_y>y))
            return rN;
    }
    return -1;
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

char* binGeohash(char* str, int count,int alpha)
{
    // char str[GEOLEN];
    itoa(count, str, 2);
    complement(str,alpha);
    return str;
}


double max_double(double x,double y)
{
	return x>y?x:y;
}

double min_double(double x,double y)
{
    return x<y?x:y;
}

int init(void)
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

// double * mat_pow(double * a_mat,int a_row,int power)
// {
//     // double * ap = (double *)a;//获取矩阵
//     int a_size = sizeof(a_mat);
//     int a_col = (int) sizeof(a_mat)/a_row;
//     int i,j;
//     double *c_mat;

//     if(power<=0){
//         puts("Error: wrong power!");
//         return NULL;
//     }
//     else
//     {
//         double * b_mat;
//         b_mat = (double *)malloc(a_size*sizeof(double));
//         for( i = 0; i < a_size; i++)
//         {
//             *(b_mat+i)=*(a_mat+i);
//         }
//         if (power!=1)
//         {
//             for (j = 0; j < power-1; j++)
//             {
//                 c_mat = dot(a_mat,b_mat,a_row,a_row);
//                 b_mat = c_mat;
//             }
//         }
//         return b_mat;
//     }
// }

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



void Init_HashTable(void)
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
