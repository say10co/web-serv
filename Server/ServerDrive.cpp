#include <iostream> 
#include "ServerDrive.hpp"
#include <arpa/inet.h> // inte_aton
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>
#include "Network.hpp"
#include <algorithm>

void send_test_response(int fd) {

	const char * resp =  "HTTP/1.1 200 OK\nDate: Mon, 27 Jul 2009 12:28:53 GMT\nServer: Apache/2.2.14 (Win32)\nLast-Modified: Wed, 22 Jul 2009 19:15:56 GMT\nContent-Length: 52\nContent-Type: text/html\nConnection: Closed\n\n<html>\n<body>\n<h1>Hello, World!</h1>\n</body>\n</html>\n";

	if (send(fd, resp, strlen(resp), 0) != (ssize_t ) strlen(resp))
		throw(ErrorLog("Send error"));
}

ServerDrive::ServerDrive(const Parse &conf): _config(conf), _fd_max(0) {
	const std::vector<Server>	&servers = this->_config.getServers(); 
	const int 					true_ = 1;
	int							sock_fd;

	FD_ZERO(&(this->_readset));
	FD_ZERO(&(this->_writeset));
	FD_ZERO(&(this->_listenset));

	for (Parse::cv_itereator cit = servers.begin(); cit != servers.end(); cit++)
	{
			sock_fd = Network::CreateSocket();
			setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(int));
			Network::BindSocket(sock_fd, cit->getPort(), cit->getAddress() );
			Network::ListenOnSocket(sock_fd);
			addSocketFd(sock_fd);
			FD_SET(sock_fd, &(this->_listenset));
			std::cerr << "Server Listening On " <<  cit->getAddress() << "[" << cit->getPort() << "]" << std::endl;
	}
}

ServerDrive::ServerDrive(const ServerDrive &server) : _config(server._config) {
	(void) server;
}

ServerDrive::~ServerDrive() {
	for(std::vector<int>::iterator it = _server_fds.begin(); it != _server_fds.end(); it++)
		close(*it);
}

void ServerDrive::io_select(fd_set &read_copy, fd_set &write_copy) {
	struct timeval timout;
	int		select_stat;

	timout.tv_sec = 5;
	timout.tv_usec = 0;
	select_stat =  select(this->_fd_max + 1, &read_copy, &write_copy, NULL, &timout);
	if  (select_stat < 0)  {
		perror(NULL);
		throw(ErrorLog("Error: Select failed"));
	}
}

void ServerDrive::addClient(Client client) {

	typedef std::map<int, Client>::value_type pair_t;

	int sock_fd = client.getConnectionFd();
	pair_t pair(sock_fd, client);
	this->_clients.insert(pair);
	addSocketFd(sock_fd);
}

void ServerDrive::addSocketFd(int fd) {
	this->_server_fds.push_back(fd);
	FD_SET(fd, &(this->_readset)); 
}

void ServerDrive::readRequest(int fd) {
	// temp until Request class is created 
	char buffer[1024];
	size_t bytes_recieved;
	Client &client = getClient(fd) ;

	bzero(buffer, sizeof(buffer));
	bytes_recieved = read(client.getConnectionFd(), buffer, sizeof(buffer)  - 1);
	std::cerr << "Receving from client : " << client.getConnectionFd()  << std::endl;

	if (bytes_recieved <= 0)  // error on while reading 
	{ 	
		if (bytes_recieved == 0) // Client closed connection
		{     
			Network::closeConnection(fd);
			FD_CLR(fd, &this->_readset);
			FD_CLR(fd, &this->_writeset);
			this->_server_fds.erase(std::find(this->_server_fds.begin(), this->_server_fds.end(), fd));
			std::cout << "EOF recieved closing..." << std::endl;
			return ;
		}
		else 
			throw(ErrorLog("Error: unable to read form sock"));
	}
	std::cout << "Recieved bytes: " << bytes_recieved << "\nData: " << buffer   << std::endl;
}


void ServerDrive::eventHandler(fd_set &read_copy, fd_set &write_copy) {

	int fd_max = this->_fd_max;
	for (int fd = 0; fd <=  fd_max; fd++) 
	{
		if (FD_ISSET(fd, &write_copy)) 		// response 
		{
			send_test_response(fd);
		 	Network::closeConnection(fd);
		 	FD_CLR(fd, &this->_readset);
		 	FD_CLR(fd, &this->_writeset);
			this->_server_fds.erase(std::find(this->_server_fds.begin(), this->_server_fds.end(), fd));
			std::cerr << "Response sent. Closing ..." << std::endl;
		}
		else if (FD_ISSET(fd, &read_copy))
		{
			if (FD_ISSET(fd, &this->_listenset)) 
				addClient(Network::acceptConnection(fd));	// new connection 
			else 											// read request 
			{
				FD_SET(fd, &(this->_writeset)); 
				readRequest(fd);
			}
		}
	}

}

void ServerDrive::run() {
	fd_set read_copy;
	fd_set write_copy;

	std::cerr << "Server Running ..." << std::endl;
	while (1) 
	{
		read_copy = this->_readset;
		write_copy = this->_writeset;
		this->_fd_max = *std::max_element(this->_server_fds.begin(), this->_server_fds.end());
		io_select(read_copy, write_copy);
		eventHandler(read_copy, write_copy);
	}
}

Client &ServerDrive::getClient(int fd) {
	clinetiterator_t it = this->_clients.find(fd);

	if (it != this->_clients.end())
		return (it->second);
	else 
		throw(ErrorLog("BUG: Potential   Server  error"));
}
