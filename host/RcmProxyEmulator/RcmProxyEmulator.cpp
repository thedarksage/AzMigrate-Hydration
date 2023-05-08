// RcmProxyEmulator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include "SecureTransport.h"
using namespace std;

//dummy server callback function
void do_serverCallback(const std::string &, const char *, size_t) {
	cout << "Hello from server callback";
	return;
}

//dummy client callback function
void do_clientCallback(const char *, size_t) {
	cout << "Hello from client callback";
	return;
}

//This function Runs Client 
//takes pointer to SecureClient class object as argument
//void function so doesn't return anything
void runClient(SecureClient* ptr) {
	ptr->run(1);
}

//This function Runs Server 
//takes pointer to SecureServer class object as argument
//void function so doesn't return anything
void RunServer(SecureServer* ptrServer) {
	ptrServer->run(2);
}

//This function Connects Client to running server
//takes pointer to SecureClient class object that is the client to be connected; ip address of server and port at which server runs as arguments
//void function so doesn't return anything
void connectClientServer(SecureClient* ptr, string ip, string port) {
	
	boost::system::error_code ec = ptr->connect(ip, port);
	if (!ec) {
		cout << "Connection Success\n";
	}
	else {
		cout << "Error occured\n";
	}
}

//This function controls the Client. The client can be closed using this.
//takes pointer to SecureClient class object as argument
//void function so doesn't return anything
void controlClient(SecureClient* ptr) {
	while (1) {
		char reply;
		std::cout << "Do you want to close client (y/n)?\n";
		std::cin >> reply;
		if (reply == 'y') {
			ptr->close();
			break;
		}
		else {
			cout << "Client still running...\n";
		}
	}
}

//This function controls the Server. The server can be closed using this.
//takes pointer to SecureServer class object as argument
//void function so doesn't return anything
void serverControlHelp(SecureServer* ptr) {
	while (1) {
		char reply;
		std::cout << "Do you want to close server (y/n)?\n";
		std::cin >> reply;
		if (reply == 'y') {
			ptr->close();
			break;
		}
		else {
			cout << "Server still running...\n";
		}
	}
}

int main(int argc, char** argv)
{
	cout << "You entered " << argc << "arguments."<<endl ;
	if (argc < 2) {
		return 0;
	}
	string choice = argv[1];
	if (choice == "server") {
		cout << "Building Server\n";

		string cert;
		string key;
		string dhFile;
		int port;

		if (argc < 6) {
			
			cout << "Enter certificate file name: ";
			getline(cin, cert);

			cout << "Enter key name: ";
			getline(cin, key);

			cout << "Enter dhFile : ";
			getline(cin, dhFile);

			cout << "Enter port : ";
			cin >> port ;

		}
		else {
			cert = argv[2];
			key = argv[3];
			dhFile = argv[4];
			port = atoi(argv[5]);
		}


		boost::function<void(const std::string &, const char *, size_t)> serverCallback;
		serverCallback = &do_serverCallback;

		//creating object of SecureServer class
		SecureServer serverObj((unsigned short)port, cert, key, dhFile, serverCallback);
		SecureServer* ptrServer = &serverObj;
		thread thread1(RunServer, ptrServer);
		thread thread2(serverControlHelp, ptrServer);
		thread2.join();
		thread1.join();
	}
	else if (choice == "client") {

		string clientID = "123";
		cout << "Client!!!" << endl;
		boost::function<void(const char *, size_t)> clientCallback;
		clientCallback = &do_clientCallback;

		SecureClient clientObj(clientID, clientCallback);
		SecureClient* ptrClient = &clientObj;

		string ip = argv[2];
		string port = argv[3];

		thread thread4(connectClientServer, ptrClient, ip, port);
		Sleep(1000);
		thread thread3(runClient, ptrClient);

		thread thread5(controlClient, ptrClient);

		thread5.join();
		thread3.join();
		thread4.join();
	}

	

	cout << "Main Over\n";

	
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
