all:
	gcc -g -c lcgrand.c
	gcc -g -c project1.c
	gcc -g -o sim1 *.o -lm 
	gcc -g -o sim2 project1-2.c -lm 

clean:
	rm -f *.o
	rm -f sim1
