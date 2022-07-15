all:
	gcc -g -pthread ./client.cpp -o client -lstdc++
	gcc -g -pthread ./server.cpp -o server -lstdc++

run_client:
	./client

run_server:
	./server

clean:
	rm client
	rm server