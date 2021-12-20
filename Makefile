shell : shell.c breakout.c
	gcc -g -o shell shell.c breakout.c

clean:
	rm -f shell
