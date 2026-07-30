INCLUDES= -I..

LIBS=-L../../../../linux/local/lib -lezApp_lib -lcJSON -llodepng  -lkissfft-float -lm

json: json.c
	gcc -o $@ -Wall -g $(INCLUDES) $^ $(LIBS)

clean:
	rm json

