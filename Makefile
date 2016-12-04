
all: brickpop

.PHONY: clean

clean:
	rm brickpop

brickpop: brickpop.c
	${CC} --std=c99 brickpop.c -o brickpop -lpng 

