FlexAlloc: FlexAlloc.c FlexAlloc.h
	gcc -g -c -Wall -m32 -fpic FlexAlloc.c
	gcc -shared -Wall -m32 -o libheap.so FlexAlloc.o

clean:
	rm -rf FlexAlloc.o libheap.so
