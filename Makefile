STD = -std=c99
ERRFLAGS = -W -Wall -pedantic-errors
OBJS = main.o mpc.o

mpc.o : mpc.c mpc.h
	clang $(STD) $(ERRFLAGS) -c mpc.c

main.o : main.c mpc.h
	clang $(STD) $(ERRFLAGS) -c main.c

main : $(OBJS)
	clang $(STD) $(ERRFLAGS) $(OBJS) -o main.exe
	rm -f $(OBJS)

