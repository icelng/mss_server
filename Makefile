objs = main.o client.o server-init.o comunication.o encdec.o mmpool.o rbtree.o tdpool.o
sql_inc = /usr/include/mysql


server:$(objs)
	gcc -o server $(objs) -lssl -lcrypto -L/usr/lib/mysql -lmysqlclient -pthread -lrt 
main.o:main.c 
	gcc -c main.c 
client.o:client.c client.h
	gcc -c client.c -I $(sql_inc) 
server-init.o:server-init.c server-init.h
	gcc -c server-init.c 
comunication.o:comunication.c
	gcc -c comunication.c 
encdec.o:encdec.c
	gcc -c encdec.c  
mmpool.o:mmpool.c
	gcc -c mmpool.c
rbtree.o:rbtree.c
	gcc -c rbtree.c
tdpool.o:tdpool.c
	gcc -c tdpool.c
	
clean:
	rm $(objs) server
	
