#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
int main() {
	char rdata[100];
	int dev = open("/dev/bmp280", O_RDWR);
	if(dev == -1) {
		printf("Opening was not possible!\n");
		return -1;
	}
	printf("Opening was successfull!\n");
	read(dev, rdata, sizeof(rdata));
	printf(" temp: %s\n",rdata);
	close(dev);
	return 0;
}
