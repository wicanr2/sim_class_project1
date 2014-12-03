all:
	gcc -c lcgrand.c
	gcc -c project1.c
	gcc -o sim1 *.o -lm 

clean:
	rm -f *.o
	rm -f sim1
