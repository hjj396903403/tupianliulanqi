#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/input.h>

static char pictures[100][100];
unsigned char *bmpmem;
static int fd_key;
static unsigned char key;
static int zuobiao[3];
static int chumo_flag;//是否按下屏幕标志位
struct input_event key_event;

#pragma pack(push) /* 将当前pack设置压栈保存 */
#pragma pack(1)    /* 必须在结构体定义之前使用,这是为了让结构体中各成员按1字节对齐 */
/* 1、需要文件信息头:14个字节 */
typedef struct tagBITMAPFILEHEADER { /* bmfh */
	unsigned short bfType;  
	unsigned long  bfSize;  
	unsigned short bfReserved1;
	unsigned short bfReserved2; 
	unsigned long  bfOffBits;  // 偏移地址
}BITMAPFILEHEADER;
/* 位图信息头 */
typedef struct tagBITMAPINFOHEADER { /* bmih */
	unsigned long  biSize;
	unsigned long  biWidth;		//x
	unsigned long  biHeight;	//y
	unsigned short biPlanes;
	unsigned short biBitCount;	//bpp
	unsigned long  biCompression;
	unsigned long  biSizeImage;
	unsigned long  biXPelsPerMeter;
	unsigned long  biYPelsPerMeter;
	unsigned long  biClrUsed;
	unsigned long  biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop) /* 恢复先前的pack设置 */

unsigned char *fbmem=NULL;
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
static int iFileSize = 0;
//poll机制的结构体
struct pollfd poll_fd[1];

void sighandler(int sig)
{//	printf("捕获的信号: %d\n",sig);	
	ioctl(fd_key,888,&key);
	printf("0x%x\n",key);
}

void show_pixel(int x,int y,int color)
{
	unsigned char *show8=NULL;
	unsigned short *show16=NULL;
	unsigned long *show32 = NULL;
	int red;
	int green;
	int blue;
	/* 定位到LCD屏上的位置 */
	show8 = fbmem + y*var.xres*var.bits_per_pixel/8 + x*var.bits_per_pixel/8;
	show16 = (unsigned short *)show8;
	show32 = (unsigned long *)show8;
	switch(var.bits_per_pixel)
	{
		case 8:
		{
			*show8 = color;
			break;
		}
		case 16:
		{
			/* RGB:565 */
			red = (color >> 16)&0xff;
			green = (color >> 8)&0xff;
			blue  = color&0xff;
			color = ((red>>3)<<11) | ((green>>2)<<6) |(blue>>3);
			*show16 = color;
			break;
		}
		case 32:
		{
			*show32 = color;
			break;
		}
		default:break;
	}
	
	
}

/*映射图片地址*/
static unsigned char *getbmpadd(char *name)
{
	unsigned char *bmpmem = NULL;
	FILE* filp;
	int fd;
	struct stat t_stat;
	/* 以r+可读可写方式打开name */
	filp = fopen(name,"r+");
	if(filp == NULL)
	{
		printf("can't open %s\n",name);
		return NULL;
	}
	/* 把文件指针转化为文件描述符 */
	fd = fileno(filp);
	/* 获取文件大小 */
	fstat(fd, &t_stat);
	iFileSize = t_stat.st_size;
	/* 映射 */
	bmpmem = mmap(NULL,iFileSize,PROT_READ|PROT_WRITE,MAP_SHARED,fd, 0);
	if(bmpmem == (unsigned char *)-1)
	{
		printf("can't mmap..\n");
		return NULL;
	}
	return bmpmem;
}
void bmp_destroy(unsigned char *bmpmem)
{
	munmap(bmpmem,iFileSize);
}

void Convert_One_Line(unsigned char *src,unsigned char *dst,int iWidth)
{
	unsigned char *show8=NULL;
	unsigned short *show16=NULL;
	unsigned long *show32 = NULL;
	unsigned char *buf = src;
	int red;
	int green;
	int blue;
	int i;
	/* 定位到LCD屏上的位置 */
	show8 = dst;
	show16 = (unsigned short *)show8;
	show32 = (unsigned long *)show8;
	for(i=0;i<iWidth;i++)
	{
		blue 	= *buf++;
		green 	= *buf++;
		red 	= *buf++;
		switch(var.bits_per_pixel)
		{
			case 16:
			{
				/* RGB:565 */
				*show16 = ((red>>3)<<11) | ((green>>2)<<6) |(blue>>3);
				show16++;
				break;
			}
			case 32:
			{
				*show32 = (red<<16)|(green<<8)|blue;
				show32++;
				break;
			}
			default:break;
		}
	
	}
}

/* 获取颜色阵列数据 */
int getbmpandshow(unsigned char *bmpmem)
{
	/* 定义文件信息头 */
	BITMAPFILEHEADER *bithead;
	/* 定义文件信息头 */
	BITMAPINFOHEADER *bitinfo;
	unsigned char *src=NULL;
	unsigned char *dst=NULL;
	int iWidth;
	int iHeight;
	int iBpp;
	int iLineWidth;
	int iRealLineWidth;
	int iFbLineWidth;
	int i=0;
	/* 获取文件信息头起始地址 */
	bithead =(BITMAPFILEHEADER *)bmpmem;
	/* 获取位图信息头起始地址 */
	bitinfo = (BITMAPINFOHEADER *)(bmpmem + sizeof(BITMAPFILEHEADER));
	iWidth  = bitinfo->biWidth;
	iHeight = bitinfo->biHeight;
	iBpp  	= bitinfo->biBitCount;
	printf("iWidth = %d\n",iWidth);
	printf("iHeight = %d\n",iHeight);
	printf("iBpp = %d\n",iBpp);
	/* 找到颜色阵列 */
	src = bmpmem + bithead->bfOffBits;
	
	/* 向4取整 */
	iLineWidth = iWidth*iBpp/8;
	iRealLineWidth = (iLineWidth+3)&~0x3;

	src += iRealLineWidth*(iHeight-1);
	
	iFbLineWidth = var.xres * var.bits_per_pixel/8;
	dst = fbmem;
	/* 显示 */
	if(24 != iBpp)
		return -1;
	for(i=0;i<iHeight;i++)
	{
		Convert_One_Line(src,dst,iWidth);
		src -= iRealLineWidth;
		dst += iFbLineWidth;
	}
	return 0;
}

int lcd_init(char *name)
{
	/* 1、打开/dev/fb0 */
	int fd;
	fd = open(name,2);
	if(fd <= 0)
	{
		printf("open is error!!\n");
		return -1;
	}
	/* 2、获取可变参数，固定参数 */
	/* 2.2、FBIOGET_VSCREENINFO获取可变参数:x,y,bpp */
	ioctl(fd,FBIOGET_VSCREENINFO,&var);
	printf("x=%d\n",var.xres);
	printf("y=%d\n",var.yres);
	printf("bpp=%d\n",var.bits_per_pixel);
	printf("oursize=%d\n",var.xres*var.yres*var.bits_per_pixel/8);
	/* 2.2、FBIOGET_FSCREENINFO获取固定参数:显存大小 */
	ioctl(fd,FBIOGET_FSCREENINFO,&fix);
	printf("size=%d\n",fix.smem_len);

	/* 3、获取显存虚拟起始地址 */
	/*
	 * start:虚拟起始地址 null 自动分配
	 * length: 映射的大小
	 * prot :权限 PROT_READ | PROT_WRITE
	 * flags : MAP_SHARED
	 * fd:文件描述符
	 */
	fbmem =(unsigned char *)mmap(NULL,fix.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fd, 0);
	if(fbmem == (unsigned char *)-1)
	{
		printf("mmap error!!!\n");
		munmap(fbmem,fix.smem_len);
		return -1;
	}
	return 0;
}

//i表示第几张图
int show_picture(int i)
{
	
	bmpmem = getbmpadd(pictures[i]); 
	if(NULL == bmpmem)
	{
		printf("can't get bmp address!!\n");
		return -1;
	}
	getbmpandshow(bmpmem); // 显示图片
	bmp_destroy(bmpmem); //释放映射的空间
}
int main(int argc,char **argv)
{
	
	int fb_iic,fb_ev;
	if(argc!=2)
	{
		printf("Usage :\n");
		printf("%s <display_car>\n",argv[0]);
		return -1;
	}
	fd_key=open("/dev/test_led_0",2);
	fb_iic=open("/dev/lcd_touch",2);
	fb_ev=open(argv[2],2);

	DIR *dir=NULL;  //目录指针
	struct dirent *dir_name=NULL; 	
  	dir=opendir(argv[1]);
  	if(dir==NULL)
   {
  	 printf("目录打开失败!\n");
  	 return -1;	
   }
	int pic_num=0;
	char temp[100][100];
	
	char *bmp_dir=NULL;
	bmp_dir=strcat(argv[1],"/");
	 printf("%s\n",bmp_dir);
	int str_len;
	while((dir_name=readdir(dir)) != NULL)
   {
   		if(strstr(dir_name->d_name,".bmp")!=NULL)
   		{
   			str_len=strlen(bmp_dir)+strlen(dir_name->d_name);
			char *file_name=malloc(str_len);
			strcat(file_name,bmp_dir);		
			strcat(file_name,dir_name->d_name);
			printf("%s\n",file_name);
			strcpy(pictures[pic_num],file_name);
			pic_num++;
			free(file_name);
       printf("j=%d %s  %s\n",pic_num,dir_name->d_name,pictures[pic_num]);
   		}
   }



	
	/*1. 绑定需要捕获的信号 */ 
	signal(SIGIO,sighandler);
	/*2. 设置pid*/	
	int f_flags;
	fcntl(fd_key,F_SETOWN,getpid());//给驱动文件描述符赋值当进程的PID号�
	/*3. 设置驱动文件支持异步IO*/
	f_flags = fcntl(fd_key,F_GETFL);  //获取当前的文件属性,返回值就是获取的属性
	fcntl(fd_key,F_SETFL,f_flags|FASYNC);   //设置当前驱动支持异步IO操作
		
	lcd_init("/dev/fb0");

	/* 4、清屏 */
	memset(fbmem,0x0,fix.smem_len);
	/* 4.1、显示图片-映射图片地址 */


	show_picture(0);
	int i;

	/*填充poll机制结构体成员*/
	poll_fd[0].fd=fb_iic;
	poll_fd[0].events=POLLIN;  //检测的事件类型

	//kk为滑动标志位
	int first_x,last_x,kk=1;
	
	while(1)
	{
		
		poll(poll_fd,1,0); //休眠事件为0秒
		if(poll_fd[0].revents & poll_fd[0].events)
		{
		//	ioctl(fb_iic,888,zuobiao);  //读取按键值
			event_sizeof=read(fb_ev,&key_event,sizeof(struct input_event));
			if(key_event.)
		//	printf("x=%d  y=%d\n",zuobiao[0],zuobiao[1]);	
		}
		if(zuobiao[2]>0)
		{
			chumo_flag=1;
			
			if(kk==1)
			{
				first_x=zuobiao[0];
				kk=0;
			}
		}
		if((zuobiao[2]==0)&&(kk==0))
		{
			last_x=zuobiao[0];
			kk=1;
		}
		if(((first_x-last_x)>100)&&(zuobiao[2]==0)&&(chumo_flag==1))
		{
			printf("first_x=%d  last_x=%d\n",first_x,last_x);	
			chumo_flag=0;
			i++;
			if(i>(pic_num-1))
			{
				i=0;
			}
			show_picture(i);
		}
		if(((last_x-first_x)>100)&&(zuobiao[2]==0)&&(chumo_flag==1))
		{
			printf("first_x=%d  last_x=%d\n",first_x,last_x);
			chumo_flag=0;
			i--;
			if(i<0)
			{
				i=pic_num-1;
			}
			show_picture(i);
		}
		if(key==0x01)
		{
			i++;
			if(i>4)
			{
				i=0;
			}
			show_picture(i);
			while(key==0x01);
		}
		
	}
	close(fd_key);
	close(fb_iic);
	return 0;
}

