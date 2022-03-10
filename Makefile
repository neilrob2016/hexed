CC=cc -g
ARGS=-Wall -Wextra -pedantic
#ARGS=-Wall -pedantic
DEPS=globals.h Makefile
OBJS=main.o keyboard.o terminal.o draw.o signals.o file.o undo.o printf.o misc.o
BIN=hexed

$(BIN): build_date $(OBJS) Makefile
	$(CC) $(OBJS) -o $(BIN)

main.o: main.c $(DEPS) build_date.h
	$(CC) $(ARGS) -c main.c

keyboard.o: keyboard.c $(DEPS)
	$(CC) $(ARGS) -c keyboard.c

terminal.o: terminal.c $(DEPS)
	$(CC) $(ARGS) -c terminal.c

draw.o: draw.c $(DEPS)
	$(CC) $(ARGS) -c draw.c

signals.o: signals.c $(DEPS)
	$(CC) $(ARGS) -c signals.c

file.o: file.c $(DEPS)
	$(CC) $(ARGS) -c file.c

undo.o: undo.c $(DEPS)
	$(CC) $(ARGS) -c undo.c

printf.o: printf.c $(DEPS)
	$(CC) $(ARGS) -c printf.c

misc.o: misc.c $(DEPS)
	$(CC) $(ARGS) -c misc.c

build_date:
	echo "#define BUILD_DATE \"`date -u +'%F %T %Z'`\"" > build_date.h

clean:
	rm -f $(BIN) *.o core* build_date.h 
