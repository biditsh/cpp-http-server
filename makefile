server: server.o work_queue.o
	gcc -pthread -o server server.o work_queue.o

server.o: server.c work_queue.h
	gcc -c server.c

work_queue.o: work_queue.c work_queue.h
	gcc -c work_queue.c

clean:
	rm server.o work_queue.o
