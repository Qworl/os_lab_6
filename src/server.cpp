#include <string.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <csignal>
#include "server.h"
#include "socket.h"
#include "wrap_zmq.h"

void* subscriber_thread(void* server){
	Server* server_ptr = (Server*) server;
	pid_t serv_pid = server_ptr->get_pid();
	try{
		pid_t child_pid = fork();
		if(child_pid == -1) throw runtime_error("Can not fork.");
		if(child_pid == 0){
			execl("client", "client", "0", server_ptr->get_publisher()->get_endpoint().data(), "-2", nullptr);
			throw runtime_error("Can not execl");
			server_ptr->~Server();
			return (void*)-1;
		}
		string endpoint = create_endpoint(EndpointType::PARENT_PUB, child_pid);
		server_ptr->get_subscriber() = new Socket(server_ptr->get_context(), SocketType::SUBSCRIBER, endpoint);
		server_ptr->get_tree().insert(0);
		for(;;){
			Message msg = server_ptr->get_subscriber()->receive();
			server_ptr->last_msg = msg;
			if(msg.command == CommandType::ERROR){
				continue;
			}
			//cout << "Message on server: " << int
			switch(msg.command){
				case CommandType::CREATE_CHILD:
					cout << "OK:" << msg.get_create_id() << endl;
					break;
				case CommandType::REMOVE_CHILD:
					cout << "OK" << endl;
					server_ptr->get_tree().delete_el(msg.get_create_id());
					break;
				case CommandType::RETURN:
					break;
				case CommandType::EXEC_CHILD:
					cout << "OK:" << msg.get_create_id() << ":" << msg.value[0] << endl;
					break;
				default:
					break;
			}
		}
	} catch(runtime_error& err){
		cout << "Server wasn't started " << err.what() << endl;
	}
	return nullptr;
}

Server::Server(){
	context = create_zmq_ctx();
	pid = getpid();
	string endpoint = create_endpoint(EndpointType::CHILD_PUB_LEFT, getpid());
	publisher = new Socket(context, SocketType::PUBLISHER, endpoint);
	if(pthread_create(&receive_msg, 0, subscriber_thread, this) != 0){
		throw runtime_error("Can not run second thread.");
	}
	working = true;
}

Server::~Server(){
	if(!working) return;
	working = false;
	send(Message(CommandType::REMOVE_CHILD, 0, 0));
	try{
		delete publisher;
		delete subscriber;
		publisher = nullptr;
		subscriber = nullptr;
		destroy_zmq_ctx(context);
		sleep(2);
	} catch (runtime_error &err){
		cout << "Server wasn't stopped " << err.what() << endl;
	}
}

pid_t Server::get_pid(){
	return pid;
}

void Server::print_tree(){
	t.print();
}

bool Server::check(int id){
	Message msg(CommandType::RETURN, id, 0);
	send(msg);
	sleep(msg_wait_time);
	msg.get_to_id() = SERVER_ID;
	//cout << msg.to_id << " " << msg.create_id << " " << msg.uniq_num << " " << endl;
	//cout << last_msg.to_id << " " << last_msg.create_id << " " << last_msg.uniq_num << " " << endl;
	return last_msg == msg;
}

void Server::send(Message msg){
	msg.to_up = false;
	publisher->send(msg);
}

Message Server::receive(){
	return subscriber->receive();
}

Socket*& Server::get_publisher(){
	return publisher;
}
	
Socket*& Server::get_subscriber(){
	return subscriber;
}

void* Server::get_context(){
	return context;
}

tree& Server::get_tree(){
	return t;
}

void Server::create_child(int id){
	if(t.find(id)){
		throw runtime_error("Error:" + to_string(id) + ":Node with that number already exists.");
	}
	if(t.get_place(id) && !check(t.get_place(id))){ //not zero node
		throw runtime_error("Error:" + to_string(id) + ":Parent node is unavailable.");
	}
	send(Message(CommandType::CREATE_CHILD, t.get_place(id), id));
	t.insert(id);
}

void Server::remove_child(int id){
	if(!t.find(id)){
		throw runtime_error("Error:" + to_string(id) + ":Node with that number doesn't exist.");
	}
	if(!check(id)){
		throw runtime_error("Error:" + to_string(id) + ":Node is unavailable.");
	}
	send(Message(CommandType::REMOVE_CHILD, id, 0));
}

void Server::exec_child(int id, int n){
	double nums[n];
	for(int i = 0; i < n; ++i){
		int cur;
		cin >> cur;
		nums[i] = cur;
	}
	if(!t.find(id)){
		throw runtime_error("Error:" + to_string(id) + ":Node with that number doesn't exist.");
	}
	if(!check(id)){
		throw runtime_error("Error:" + to_string(id) + ":Node is unavailable.");
	}
	send(Message(CommandType::EXEC_CHILD, id, n, nums, 0));
}

void process_cmd(Server& server, string cmd){
	if(cmd == "create"){
		int id;
		cin >> id;
		server.create_child(id);
	} else if (cmd == "remove"){
		int id;
		cin >> id;
		server.remove_child(id);
	} else if (cmd == "exec"){
		int id;
		cin >> id;
		int n;
		cin >> n;
		server.exec_child(id, n);
	} else if(cmd == "exit"){
		throw invalid_argument("Exiting...");
	} else if(cmd == "pingall"){
		vector<int> tmp = server.get_tree().get_all_elems();
		//tmp.pop_back();
		cout << "OK:";
		bool fnd = false;
		for(int& i : tmp){
			//cout << "(()()" << i << endl;
			if(!server.check(i)){
				if(fnd) cout << ';';
				cout << i;
				fnd = true;
			}
		}
		if(!fnd) cout << "-1";
		cout << endl;
	} else if(cmd == "status"){
		int id;
		cin >> id;
		if(!server.get_tree().find(id)){
			throw runtime_error("Error:" + to_string(id) + ":Node with that number doesn't exist.");
		}
		if(server.check(id)){
			cout << "OK" << endl;
		} else{
			cout << "Node is unavailable" << endl;
		}
	} else {
		cout << "It is not a command!\n";
	}
}

Server* server_ptr = nullptr;
void TerminateByUser(int) {
	if (server_ptr != nullptr) {
		server_ptr->~Server();
	}
	cout << to_string(getpid()) + " Terminated by user" << endl;
	exit(0);
}

int main (int argc, char const *argv[]) 
{
	try{
		if (signal(SIGINT, TerminateByUser) == SIG_ERR) {
			throw runtime_error("Can not set SIGINT signal");
		}
		if (signal(SIGSEGV, TerminateByUser) == SIG_ERR) {
			throw runtime_error("Can not set SIGSEGV signal");
		}
		if (signal(SIGTERM, TerminateByUser) == SIG_ERR) {
			throw runtime_error("Can not set SIGTERM signal");
		}
		Server server;
		server_ptr = &server;
		cout << getpid() << " server started correctly!\n";
		for(;;){
			try{
				string cmd;
				while(cin >> cmd){
					process_cmd(server, cmd);
				}
			} catch(const runtime_error& arg){
				cout << arg.what() << endl;
			}
		}
	} catch(const runtime_error& arg){
		cout << arg.what() << endl;
	} catch(...){}
	sleep(5);
	return 0;
}
