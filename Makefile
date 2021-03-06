ADDR=/work/linux-3.5/linux-3.5
FILE=/work/rootfs/test

all:lcd_bmp.c
	echo $(ABC)
	@make -C $(ADDR) M=`pwd` modules
	@cp *.ko $(FILE) -v
	@make -C $(ADDR) M=`pwd` modules clean
	@arm-linux-gcc lcd_bmp.c -o app
	@cp app $(FILE) -v
	@rm app -fv
	
obj-m +=led_driver.o iic_dev.o iic_drv.o

