spider:main.c spider.c spider.h adns.c adns.h
	gcc main.c spider.c adns.c -g -lpthread -lz -o spider
clean:
	rm -fr *~
	rm -fr *.com
	rm -fr *.cn
