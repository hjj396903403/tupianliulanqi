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
static int chumo_flag;//ÊÇ·ñ°´ÏÂÆÁÄ»±êÖ¾Î»
struct input_event key_event;

#pragma pack(push) /* ½«µ±Ç°packÉèÖÃÑ¹Õ»±£´æ */
#pragma pack(1)    /* ±ØÐëÔÚ½á¹¹Ìå¶¨ÒåÖ®Ç°Ê¹ÓÃ,ÕâÊÇÎªÁËÈÃ½á¹¹ÌåÖÐ¸÷³ÉÔ±°´1×Ö½Ú¶ÔÆë */
/* 1¡¢ÐèÒªÎÄ¼þÐÅÏ¢Í·:14¸ö×Ö½Ú */
typedef struct tagBITMAPFILEHEADER { /* bmfh */
	unsigned short bfType;  
	unsigned long  bfSize;  
	unsigned short bfReserved1;
	unsigned short bfReserved2; 
	unsigned long  bfOffBits;  // Æ«ÒÆµØÖ·
}BITMAPFILEHEADER;
/* Î»Í¼ÐÅÏ¢Í· */
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
#pragma pack(pop) /* »Ö¸´ÏÈÇ°µÄpackÉèÖÃ */

unsigned char *fbmem=NULL;
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
static int iFileSize = 0;
//poll»úÖÆµÄ½á¹¹Ìå
struct pollfd poll_fd[1];

void sighandler(int sig)
{//	printf("²¶»ñµÄÐÅºÅ: %d\n",sig);	
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
	/* ¶¨Î»µ½LCDÆÁÉÏµÄÎ»ÖÃ */
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

/*Ó³ÉäÍ¼Æ¬µØÖ·*/
static unsigned char *getbmpadd(char *name)
{
	unsigned char *bmpmem = NULL;
	FILE* filp;
	int fd;
	struct stat t_stat;
	/* ÒÔr+¿É¶Á¿ÉÐ´·½Ê½´ò¿ªname */
	filp = fopen(name,"r+");
	if(filp == NULL)
	{
		printf("can't open %s\n",name);
		return NULL;
	}
	/* °ÑÎÄ¼þÖ¸Õë×ª»¯ÎªÎÄ¼þÃèÊö·û */
	fd = fileno(filp);
	/* »ñÈ¡ÎÄ¼þ´óÐ¡ */
	fstat(fd, &t_stat);
	iFileSize = t_stat.st_size;
	/* Ó³Éä */
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
	/* ¶¨Î»µ½LCDÆÁÉÏµÄÎ»ÖÃ */
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

/* »ñÈ¡ÑÕÉ«ÕóÁÐÊý¾Ý */
int getbmpandshow(unsigned char *bmpmem)
{
	/* ¶¨ÒåÎÄ¼þÐÅÏ¢Í· */
	BITMAPFILEHEADER *bithead;
	/* ¶¨ÒåÎÄ¼þÐÅÏ¢Í· */
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
	/* »ñÈ¡ÎÄ¼þÐÅÏ¢Í·ÆðÊ¼µØÖ· */
	bithead =(BITMAPFILEHEADER *)bmpmem;
	/* »ñÈ¡Î»Í¼ÐÅÏ¢Í·ÆðÊ¼µØÖ· */
	bitinfo = (BITMAPINFOHEADER *)(bmpmem + sizeof(BITMAPFILEHEADER));
	iWidth  = bitinfo->biWidth;
	iHeight = bitinfo->biHeight;
	iBpp  	= bitinfo->biBitCount;
	printf("iWidth = %d\n",iWidth);
	printf("iHeight = %d\n",iHeight);
	printf("iBpp = %d\n",iBpp);
	/* ÕÒµ½ÑÕÉ«ÕóÁÐ */
	src = bmpmem + bithead->bfOffBits;
	
	/* Ïò4È¡Õû */
	iLineWidth = iWidth*iBpp/8;
	iRealLineWidth = (iLineWidth+3)&~0x3;

	src += iRealLineWidth*(iHeight-1);
	
	iFbLineWidth = var.xres * var.bits_per_pixel/8;
	dst = fbmem;
	/* ÏÔÊ¾ */
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
	/* 1¡¢´ò¿ª/dev/fb0 */
	int fd;
	fd = open(name,2);
	if(fd <= 0)
	{
		printf("open is error!!\n");
		return -1;
	}
	/* 2¡¢»ñÈ¡¿É±ä²ÎÊý£¬¹Ì¶¨²ÎÊý */
	/* 2.2¡¢FBIOGET_VSCREENINFO»ñÈ¡¿É±ä²ÎÊý:x,y,bpp */
	ioctl(fd,FBIOGET_VSCREENINFO,&var);
	printf("x=%d\n",var.xres);
	printf("y=%d\n",var.yres);
	printf("bpp=%d\n",var.bits_per_pixel);
	printf("oursize=%d\n",var.xres*var.yres*var.bits_per_pixel/8);
	/* 2.2¡¢FBIOGET_FSCREENINFO»ñÈ¡¹Ì¶¨²ÎÊý:ÏÔ´æ´óÐ¡ */
	ioctl(fd,FBIOGET_FSCREENINFO,&fix);
	printf("size=%d\n",fix.smem_len);

	/* 3¡¢»ñÈ¡ÏÔ´æÐéÄâÆðÊ¼µØÖ· */
	/*
	 * start:ÐéÄâÆðÊ¼µØÖ· null ×Ô¶¯·ÖÅä
	 * length: Ó³ÉäµÄ´óÐ¡
	 * prot :È¨ÏÞ PROT_READ | PROT_WRITE
	 * flags : MAP_SHARED
	 * fd:ÎÄ¼þÃèÊö·û
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

//i±íÊ¾µÚ¼¸ÕÅÍ¼
int show_picture(int i)
{
	
	bmpmem = getbmpadd(pictures[i]); 
	if(NULL == bmpmem)
	{
		printf("can't get bmp address!!\n");
		return -1;
	}
	getbmpandshow(bmpmem); // ÏÔÊ¾Í¼Æ¬
	bmp_destroy(bmpmem); //ÊÍ·ÅÓ³ÉäµÄ¿Õ¼ä
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

	DIR *dir=NULL;  //Ä¿Â¼Ö¸Õë
	struct dirent *dir_name=NULL; 	
  	dir=opendir(argv[1]);
  	if(dir==NULL)
   {
  	 printf("Ä¿Â¼´ò¿ªÊ§°Ü!\n");
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



	
	/*1. °ó¶¨ÐèÒª²¶»ñµÄÐÅºÅ */ 
	signal(SIGIO,sighandler);
	/*2. ÉèÖÃpid*/	
	int f_flags;
	fcntl(fd_key,F_SETOWN,getpid());//¸øÇý¶¯ÎÄ¼þÃèÊö·û¸³Öµµ±½ø³ÌµÄPIDºÅ¡
	/*3. ÉèÖÃÇý¶¯ÎÄ¼þÖ§³ÖÒì²½IO*/
	f_flags = fcntl(fd_key,F_GETFL);  //»ñÈ¡µ±Ç°µÄÎÄ¼þÊôÐÔ,·µ»ØÖµ¾ÍÊÇ»ñÈ¡µÄÊôÐÔ
	fcntl(fd_key,F_SETFL,f_flags|FASYNC);   //ÉèÖÃµ±Ç°Çý¶¯Ö§³ÖÒì²½IO²Ù×÷
		
	lcd_init("/dev/fb0");

	/* 4¡¢ÇåÆÁ */
	memset(fbmem,0x0,fix.smem_len);
	/* 4.1¡¢ÏÔÊ¾Í¼Æ¬-Ó³ÉäÍ¼Æ¬µØÖ· */


	show_picture(0);
	int i;

	/*Ìî³äpoll»úÖÆ½á¹¹Ìå³ÉÔ±*/
	poll_fd[0].fd=fb_iic;
	poll_fd[0].events=POLLIN;  //¼ì²âµÄÊÂ¼þÀàÐÍ

	//kkÎª»¬¶¯±êÖ¾Î»
	int first_x,last_x,kk=1;
	
	while(1)
	{
		
		poll(poll_fd,1,0); //ÐÝÃßÊÂ¼þÎª0Ãë
		if(poll_fd[0].revents & poll_fd[0].events)
		{
		//	ioctl(fb_iic,888,zuobiao);  //¶ÁÈ¡°´¼üÖµ
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

