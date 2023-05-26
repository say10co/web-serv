#ifndef SERVERDRIVE_HPP
# define SERVERDRIVE_HPP 

#include  "../config/ConfigParse/inc/parse.hpp"
#include "Client.hpp"
#include <map>

class   ServerDrive {

	private:
		const Parse&				_config;
		fd_set						_readset;
		fd_set 		   				_writeset;
		fd_set 		   			 	_listenset;
		int			   				_fd_max;
		std::vector<int> 			_server_fds;
		std::map<int, Client> 		_clients;

		size_t 						_client_timeout;

	public:
		static const std::string HEADER_DELIM;
		static const std::string CRLF;

	public:
		typedef std::map<int, Client>::iterator clinetiterator_t ;
		ServerDrive(const Parse &conf);
		ServerDrive(const ServerDrive &server);
		~ServerDrive();

		void addClient(Client client);
		void addSocketFd(int fd);
		void run();

		Client &getClient(int fd);

		void io_select(fd_set &read_copy, fd_set &write_copy);
		void eventHandler(fd_set &read_copy, fd_set &write_copy);
		void readRequest(int fd);
		bool unchunkBody(HttpRequest &request);

		void CheckRequestStatus(Client &client);
		void CloseConnection(int fd);
		void checkClientTimout(int fd);

		void getHeader(HttpRequest &request);
		bool getBody(HttpRequest &client_request);

};

#endif /* SERVERDRIVE_HPP */