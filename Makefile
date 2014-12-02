all:
	gcc -c lcgrand.c
	gcc -c project1.c
	gcc -o sim1 *.o

clean:
	rm *.o
