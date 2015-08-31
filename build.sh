g++ -std=c++11 server.cpp -o server -I./include -L/usr/lib -lpthread -lcoap -loctbstack
g++ -std=c++11 client.cpp -o client -I./include -L/usr/lib -lpthread -lcoap -loctbstack