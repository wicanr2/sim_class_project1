all:
	gcc -g -c lcgrand.c
	gcc -g -c project1.c
	gcc -g -o sim1 *.o -lm 

clean:
	rm -f *.o
	rm -f sim1
