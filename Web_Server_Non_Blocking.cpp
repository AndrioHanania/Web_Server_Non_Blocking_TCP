#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>

struct SocketState
{
	SOCKET id;			
	int	recv;		
	int	send=-1;		
	int sendSubType = -1;
	char buffer[1000] = {"\0"};
	int len=-1;
	string FileName;
	string data;
	clock_t StartTime;	// Used to measure timeout 
	int countReq = 0;
};


const int PORT = 8080;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;

const int GET = 1;
const int POST = 2;
const int HEAD = 3;
const int PUT = 4;
const int DEL = 5;
const int TRACE = 6;
const int OPTIONS = 7;

string getMessageFromBodyRequest();

bool addSocket(SOCKET id, int what, SocketState sockets[], int socketsCount);
void removeSocket(int index, SocketState sockets[], int socketsCount);
void acceptConnection(int index, SocketState sockets[], int socketsCount);
void receiveMessage(int index, SocketState sockets[], int socketsCount);
void sendMessage(int index, SocketState sockets[], int socketsCount);


int main()
{
	cout << "Web server: Running...\n";

	struct SocketState sockets[MAX_SOCKETS] = { 0 };
	int socketsCount = 0;

	// Initialize Winsock (Windows Sockets).
	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Web Server: Error at WSAStartup()\n";
		return -1;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Web Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return -1;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).
	serverService.sin_port = htons(PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Web Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return -1;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Web Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return -1;
	}
	addSocket(listenSocket, LISTEN, sockets, socketsCount);

	// Accept connections and handles them one by one.
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		//
		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.
		//
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Web Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return -1;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i, sockets, socketsCount);
					break;

				case RECEIVE:
					receiveMessage(i, sockets, socketsCount);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i, sockets, socketsCount);
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "Web Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
	return 1;
}

bool addSocket(SOCKET id, int what, SocketState sockets[], int socketsCount)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			//
			//set the socket to be in non-blocking mode
			//
			unsigned long flag = 1;
			if (ioctlsocket(id, FIONBIO, &flag) != 0) 
			{
				cout << "Web Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
			}
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index, SocketState sockets[], int socketsCount)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	sockets[index].len = 0; 
	socketsCount--;
}

void acceptConnection(int index, SocketState sockets[], int socketsCount)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Web Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Web Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	if (addSocket(msgSocket, RECEIVE, sockets, socketsCount) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index, SocketState sockets[], int socketsCount)
{
	SOCKET msgSocket = sockets[index].id;


	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);
	sockets[index].countReq++;

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Web Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index, sockets, socketsCount);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index, sockets, socketsCount);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Web Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		char req[1000];
		strcpy(req, &sockets[index].buffer[len]);
		string method = strtok(&sockets[index].buffer[len], " /"); //Gets the http method from request;
		strcpy(&sockets[index].buffer[len], req);
		char* reqDynamic = &req[0];

		if (sockets[index].len > 0)
		{
			if(strcmp(method.c_str(), "GET") == 0 || strcmp(method.c_str(), "HEAD") == 0)
			{
				//char* reqDynamic = &req[0];
				if (strcmp(method.c_str(), "GET") == 0)
					reqDynamic +=  6;
				else
					reqDynamic += 7;

				if (strncmp(reqDynamic, "HTTP", 4) != 0)
				{
					string QueryParams = strtok(reqDynamic, " ");
					//Let's say for now that QueryParams is "lnag=**" and thats it.!!!!!!!!!
					string lang = QueryParams.substr(5);

					if (strcmp(lang.c_str(), "en") == 0)
					{
						sockets[index].FileName = "English.html";
					}
					else if (strcmp(lang.c_str(), "he") == 0)
					{
						sockets[index].FileName = "Hebrew.html";
					}
					else if (strcmp(lang.c_str(), "fr") == 0)
					{
						sockets[index].FileName = "French.html";
					}
				}
				else //default QueryParams
				{
					sockets[index].FileName = "English.html";
				}

				if(strcmp(method.c_str(), "GET") == 0)
					sockets[index].sendSubType = GET;
				else 
					sockets[index].sendSubType = HEAD;
			}
			else if (strcmp(method.c_str(), "POST") == 0)
			{
				sockets[index].data = getMessageFromBodyRequest();
				sockets[index].sendSubType = POST;
			}
			else if (strcmp(method.c_str(), "PUT") == 0)
			{
				string newFileName = strtok(NULL, "/ ");
				sockets[index].FileName = newFileName;
				sockets[index].data = getMessageFromBodyRequest();
				sockets[index].sendSubType = PUT;
			}
			else if (strcmp(method.c_str(), "DELETE") == 0)
			{
				string newFileName = strtok(NULL, "/ ");
				sockets[index].FileName = newFileName;
				sockets[index].sendSubType = DEL;
			}
			else if (strcmp(method.c_str(), "OPTIONS") == 0)
			{
				sockets[index].sendSubType = OPTIONS;
			}
			else if (strcmp(method.c_str(), "TRACE") == 0)
			{
				string fileName = strtok(NULL, "/ ");
				sockets[index].FileName = fileName;
				sockets[index].sendSubType = TRACE;
			}

		}
	}

	sockets[index].send = SEND;
	memcpy(sockets[index].buffer, &sockets[index].buffer[bytesRecv], sockets[index].len - bytesRecv);
	sockets[index].len -= bytesRecv;
	sockets[index].StartTime = clock();
}


string getMessageFromBodyRequest()
{
	string strToPrint;
	bool isMessageValid=false;
	while (strToPrint != "\r")
	{
		strToPrint = strtok(NULL, "\n\0");
		if (strncmp(strToPrint.c_str(), "Content-Length:", 15) == 0)
		{
			string contentLength = strToPrint.substr(16);
			contentLength.pop_back();
			stringstream num(contentLength);
			int x = -1;
			num >> x;
			if (x == 0)
			{
				isMessageValid = false;
			}
			else if (x > 0)
			{
				isMessageValid = true;
			}
		}
	}

	if (isMessageValid)
		strToPrint = strtok(NULL, "");
	else
		strToPrint = "";
	return strToPrint;
}



void sendMessage(int index, SocketState sockets[], int socketsCount)
{
	int bytesSent = 0;
	char sendBuff[255];
	strcpy(sendBuff, "");
	SOCKET msgSocket = sockets[index].id;
	float timePassed = ((float)clock() - sockets[index].StartTime) / CLOCKS_PER_SEC;

	if (timePassed > 120)
	{
		sprintf(sendBuff, "HTTP/1.1 408 Request Timeout\r\nconnection: close\r\nContent-Length: 0\n\n");
	}
	else {
		if (sockets[index].sendSubType == GET)
		{
			string fileName = "C:\\Temp\\";
			if(sockets[index].FileName != "")
				fileName.append(sockets[index].FileName);
			else 
				fileName.append("English.html");
			string content;
			char c;
			FILE* file;
			file = fopen(fileName.c_str(), "r");
			if (file != NULL) {
				while ((c = getc(file)) != EOF)
					content.push_back(c);
				fclose(file);
			}
				
			string tosend = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
			int size = content.length();
			string s = std::to_string(size);
			tosend += s;
			tosend += "\r\nConnection: keep-alive\n\n";
			tosend += content;
			const char* message = tosend.c_str();
			strcpy(sendBuff, message);
		}
		else if (sockets[index].sendSubType == POST)
		{
			cout << "The string to print from the request(body): " << sockets[index].data << "." << endl;
			strcpy(sendBuff, "HTTP/1.1 200 OK\r\n\Content-Type: text/html\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");
		}
		else if (sockets[index].sendSubType == PUT)
		{
			string fileName = "C:\\Temp\\";
			fileName.append(sockets[index].FileName);
			ofstream newFile(fileName, ios_base::_Nocreate);
			if (!newFile)//firt time or some error
			{
				ofstream newFile2(fileName);
				if (!newFile2)//some error
				{
					newFile.close();
					cout << "Error with opening the new file";
					strcpy(sendBuff, "HTTP/1.1 501 Not Implemented\r\n\Content-Type: text/html\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");
				}
				else//firt time
				{
					newFile2 << sockets[index].data;
					newFile2.close();
					strcpy(sendBuff, "HTTP/1.1 201 Created\r\n\Content-Type: text/html\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");

				}
			}
			else//after first time
			{
				newFile << sockets[index].data;
				newFile.close();
				strcpy(sendBuff, "HTTP/1.1 204 No Content\r\n\Content-Type: text/html\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");
			}
		}
		else if (sockets[index].sendSubType == DEL)
		{
			string fileName = "C:\\Temp\\";
			fileName.append(sockets[index].FileName);
			ofstream newFile(fileName, ios_base::_Nocreate);
			if (!newFile)
			{
				strcpy(sendBuff, "HTTP/1.1 204 No Content\r\n\Content-Type: text/html\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");
			}
			else
			{
				newFile.close();
				if (remove(fileName.c_str()) != 0)//Error deleting file
				{
					strcpy(sendBuff, "HTTP/1.1 501 Not Implemented\r\n\Content-Type: text/html\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");
				}
				else//File successfully deleted
				{
					strcpy(sendBuff, "HTTP/1.1 202 Accepted\r\n\Content-Type: text/html\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");
				}
			}
		}
		else if (sockets[index].sendSubType == OPTIONS)
		{
			strcpy(sendBuff, "HTTP/1.1 200 Ok\r\n\Content-Type: text/html\r\nAllow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\nContent-Length: 0\r\nConnection: keep-alive\n\n");
		}
		else if (sockets[index].sendSubType == HEAD)
		{
			string fileName = "C:\\Temp\\";
			fileName.append(sockets[index].FileName);

			string content;
			char c;
			FILE* file;
			file = fopen(fileName.c_str(), "r");
			while ((c = getc(file)) != EOF)
				content.push_back(c);

			string tosend = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
			int size = content.length();
			string s = std::to_string(size);
			tosend += s;
			tosend += "\r\nConnection: keep-alive\n\n";
			const char* message = tosend.c_str();
			strcpy(sendBuff, message);
		}
		else if (sockets[index].sendSubType == TRACE)
		{
			string path = "C:\\Temp\\";
			path.append(sockets[index].FileName);
			if (strcmp(sockets[index].FileName.c_str(), "HTTP") == 0)
			{
				cout << "Web server: Error with request";
				strcpy(sendBuff, "HTTP/1.1 400 Bad Request\r\ncontent-type:text/html\r\nContent-Length: 0\n\n");
			}
			else
			{
				ofstream newFile(path, ios_base::_Nocreate);
				if (!newFile)//firt time or some error
				{
					strcpy(sendBuff, "HTTP/1.1 404 Not Found\r\ncontent-type:text/html\r\nContent-Length: 0\n\n");
				}
				else
				{
					sprintf(sendBuff, "HTTP/1.1 200 OK\r\ncontent-type:text/html\r\nContent-Length: %d\n\n%s", path.length(), path.c_str());
				}
			}
		}
	}

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	sockets[index].countReq--;

	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Web Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}


	if(sockets[index].countReq == 0)
		sockets[index].send = IDLE;
}
