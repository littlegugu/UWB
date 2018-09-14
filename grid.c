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
#define GEOLEN 20
#define BASE32_LEN 5
#define BASE32_LIM 8
#define BASE32_CODE 10
#define LENF 8

typedef struct {
    double top_x;
	double low_x;
	double top_y;
	double low_y;
    int wall_num;
}wall;

// struct wall;
typedef struct {
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
    wall walls[6];
}room;

typedef struct {
    double top_x;
    double low_x;
    double top_y;
    double low_y;
}area;


room rooms[5];//声明五个房间
area areas;
// double area[4];
double door_x = 0.0;
double door_y = 0.0;
double wall_x = 0.0;
double wall_y = 0.0;

static const char base32_alphabet[32] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'b', 'c', 'd', 'e', 'f', 'g',
        'h', 'j', 'k', 'm', 'n', 'p', 'q', 'r',
        's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

void split(char * data,double *list);
double * dot(double * a_mat, double * b_mat,int a_row,int b_row);
double * mat_pow(double * a_mat,int a_row,int power);
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
int main(int argc, char const *argv[])
{

    init();
    // int * areaGridDec;
    // areaGridDec = (int *)malloc( 1000 *sizeof(int));
    // int areaGridDec[] = {0};
    int *  areaGridDec = geohash_grid();
    time_t t;
	t = time(NULL);
	int ii = time(&t);
    printf("%d\n",areaGridDec[0]);
    int jj = time(&t);
    printf("time:%d\n",jj-ii);
    printf("areaGridDec:%d byte",_msize(areaGridDec)); 
    // cout<<_msize(areaGridDec)<<endl;
    getchar();
    return 0;
}






int * geohash_grid(void)
{
    //动态计算精度范围
    double x_range[2]={0,0};//0:下界；1:上界;
    double y_range[2]={0,0};
    int alpha=0;//二分次数
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
    int index;
    int *area_num;
    int index_lim = (pow(2,alpha))*(pow(2,alpha));
    area_num = (int *)malloc( index_lim *sizeof(int));
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
            printf("index:%d\n",index);
            area_num[index] = 1;
            // printf("%d\n",index);
            // fprintf(fp,"x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,binToDec(binStr));
            // printf("x:%f,y:%f == Geohash=%s,decimal base=%d\n",x,y, geostr,binToDec(binStr));
        }
        
    }
    // fclose(fp);
    return area_num;
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
		puts("file reading...");
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
    fp = fopen("D:\\program\\UWB\\data\\area1.txt","r+");
    
    if (fp!=NULL) {
        fgets(buf,LINE,fp);
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

double * dot(double * a_mat, double * b_mat,int a_row,int b_row)
{
    int a_size  = sizeof(a_mat);
    int b_size  = sizeof(b_mat);
    int a_col   = (int)a_size/a_row; 
    int b_col   = (int)b_size/b_row; 
    if(a_col != b_row)
    {
        printf("Error:Two matrix can not be be multiplied!");
        return NULL;
    }else{
        // double * ap = (double *)a;//获取矩阵
        // double * bp = (double *)b;//获取矩阵a的首地址 
        double value;
        int i,j,k;
        const int  c_size = a_row * b_col;
        double *c_mat;

        // printf("c_size:%d\n",c_size);
        c_mat = (double *)malloc(sizeof(c_size));
        for( i = 0; i < a_row; i++)
        {
            for( j = 0; j < b_col; j++)
            {
                value = 0.0;
                for( k = 0; k < b_row; k++)
                {
                    value += ((*(a_mat+(j*a_row)+k))*(*(b_mat+(k*a_row)+i)));
                }
                *(c_mat+i+(j*a_row))=value;
            }
        }
        return c_mat;
    }
}

double * mat_pow(double * a_mat,int a_row,int power)
{
    // double * ap = (double *)a;//获取矩阵
    int a_size = sizeof(a_mat);
    int a_col = (int) sizeof(a_mat)/a_row;
    int i,j;
    double *c_mat;

    if(power<=0){
        puts("Error: wrong power!");
        return NULL;
    }
    else
    {
        double * b_mat;
        b_mat = (double *)malloc(a_size*sizeof(double));
        for( i = 0; i < a_size; i++)
        {
            *(b_mat+i)=*(a_mat+i);
        }
        if (power!=1)
        {
            for (j = 0; j < power-1; j++)
            {
                c_mat = dot(a_mat,b_mat,a_row,a_row);
                b_mat = c_mat;
            }
        }
        return b_mat;
    }
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




