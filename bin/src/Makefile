
all: ../ezsh ezlogfilter 

../ezsh: ezsh.c ../../src/ezApp_lib/utils_openssl.c
	gcc -I ../../src/ezApp_lib/include -g -Wall -o ../ezsh $^ -lreadline -lcrypto

ezlogfilter: ezlogfilter.c
	gcc -g -Wall -o ezlogfilter ezlogfilter.c -lreadline

clean:
	rm -f ../ezsh ezlogfilter
