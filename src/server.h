#ifndef _SERVER_H
#define _SERVER_H

#include "tree.h"
#include "socket.h"
#include "wrap_zmq.h"

#define msg_wait_time 1

class Server{
public:
	Server();
	~Server();
	void print_tree();
	void send(Message msg);
	Message receive();
	void create_child(int id);
	void remove_child(int id);
	void exec_child(int id, int n);
	pid_t get_pid();
	bool check(int id);
	Socket*& get_publisher();
	Socket*& get_subscriber();
	void* get_context();
	tree& get_tree();
	Message last_msg;
private:
	pid_t pid;
	tree t;
	void *context = nullptr;
	Socket* publisher;
	Socket* subscriber;
	bool working;
	pthread_t receive_msg;
};

#endif