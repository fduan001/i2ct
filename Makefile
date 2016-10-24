CC ?= gcc
all: i2ct

i2ct: i2c.c
	${CC} -O3 -g -o i2ct i2c.c

clean:
	rm i2ct >/dev/null 2>&1

