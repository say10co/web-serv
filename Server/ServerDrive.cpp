/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerDrive.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bberkass <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/21 23:15:29 by adriouic          #+#    #+#             */
/*   Updated: 2023/06/23 03:55:38 by adriouic         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "ServerDrive.hpp"
#include <strings.h>
#include "ErrorHandler.hpp"
#include "Network.hpp"
#include <algorithm>
#include "Utils.hpp"
#include <unistd.h>

const std::string ServerDrive::HEADER_DELIM = "\r\n\r\n";
const std::string ServerDrive::CRLF			= "\r\n";

// Constructors 
ServerDrive::ServerDrive(Parse &conf): _config(conf),
											_virtual_servers(conf.getVirtualServers()),
											_fd_max(0),
											_client_timeout(5) {

		std::vector<Server>	&servers = this->_config.getVirtualServers(); 
		const int 					true_ = 1;
		int							sock_fd;

		FD_ZERO(&(this->_readset));
		FD_ZERO(&(this->_writeset));
		FD_ZERO(&(this->_listenset));
		for (Parse::v_iterator server = servers.begin(); server != servers.end(); server++)
		{
			sock_fd = Network::CreateSocket();
			setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(int));
			Network::BindSocket(sock_fd, server->getPort(), server->getAddress() );

			Network::ListenOnSocket(sock_fd);
			addSocketFd(sock_fd);
			FD_SET(sock_fd, &(this->_listenset)); {
				const std::string log = "Serving HTTP on " + server->getAddress() + " Port: " +  std::to_string(server->getPort());
				ConsoleLog::Specs(log) ;
			}
			server->setSocketFd(sock_fd);
		}
}

ServerDrive::ServerDrive(const ServerDrive &server) :_config(server._config),
													 _virtual_servers(server._virtual_servers){
}

ServerDrive::~ServerDrive() {
	for(std::vector<int>::iterator it = _server_fds.begin(); it != _server_fds.end(); it++)
		close(*it);
}

void ServerDrive::io_select(fd_set &read_copy, fd_set &write_copy) {
	
	struct timeval timout;
	int		select_stat;

	timout.tv_sec = 4;
	timout.tv_usec = 0;

	#if DEBUG
	ConsoleLog::Debug("Selecting socket fds ");
	#endif 
	select_stat =  select(this->_fd_max + 1, &read_copy, &write_copy, NULL, NULL);//&timout);
	if  (select_stat < 0)  {
		#if DEBUG
			perror(NULL);
		ConsoleLog::Error("Select failed ");
		#endif
		throw(RequestError(ErrorNumbers::_500_INTERNAL_SERVER_ERROR));
	}
}

void ServerDrive::addClient(Client client) {

	typedef std::map<int, Client>::value_type pair_t;

	int sock_fd = client.getConnectionFd();
	int server_fd = client.getServerFd();
	client.setServer(getServerByFd(server_fd));
	pair_t pair(sock_fd, client);
	this->_clients.insert(pair);
	addSocketFd(sock_fd);
}

void ServerDrive::addSocketFd(int fd) {
	this->_server_fds.push_back(fd);
	FD_SET(fd, &(this->_readset)); 
}

void ServerDrive::readRequest(int fd) {
	Client &client				= getClient(fd) ;
	char *buffer				= client._buffer;
	size_t client_buffer_size	= sizeof(client._buffer);
	size_t bytes_recieved;

	bzero(buffer, client_buffer_size);
	bytes_recieved = read(client.getConnectionFd(), buffer, client_buffer_size - 1);
	client.setRequestTimout(time(NULL));
	if (bytes_recieved <= 0) {			// error on while reading 
		if (bytes_recieved == 0) { 		// Client closed connection
			Network::closeConnection(fd);
			CloseConnection(fd);
			ConsoleLog::Warning("EOF recieved closing...");
			return ;
		}
		else 
			throw(RequestError(ErrorNumbers::_500_INTERNAL_SERVER_ERROR));
	}
	client.saveRequestData(bytes_recieved); // PUSH BUFFER TO REQUEST MASTER BUFFER 
	CheckRequestStatus(client);
	#if DEBUG
	ConsoleLog::Warning("Receiving from Client: " + std::to_string(fd));
	#endif 
}

void ServerDrive::getHeader(HttpRequest &request)  {
	std::string &request_data = request.getRequestData();
	size_t header_pos = request_data.find(HEADER_DELIM);

	if (header_pos != std::string::npos) {
		std::string header = request_data.substr(0, header_pos);
		std::string rest = request_data.substr(header_pos + HEADER_DELIM.size());
		request.parse(header);
		request_data = rest;
	}
	if (request_data.empty() && (request.getRequestMethod() == HttpRequest::GET 
								|| request.getRequestMethod() == HttpRequest::DELETE) )
		request.setRequestState(HttpRequest::REQUEST_READY);
}

bool ServerDrive::unchunkBody(HttpRequest &request) {
	size_t		size_pos;
	size_t		chunk_size;
	std::string hex_sting;
	std::string &request_body = request.getRequestData();

	size_pos = request_body.find(ServerDrive::CRLF);
	if (size_pos == std::string::npos)
		return (false);

	hex_sting = request_body.substr(0, size_pos);
	if (!Utils::is_hex(hex_sting))
		throw (RequestError(ErrorNumbers::_400_BAD_REQUEST));

	chunk_size = Utils::hexStringToSizeT(hex_sting);
	if (chunk_size > (request_body.size() - (hex_sting.size() + CRLF.size() )))
		return false;									// READ REST OF CHUNK

	request_body = request_body.substr(size_pos + CRLF.size());
	if (chunk_size == 0) {
		request.setRequestState(HttpRequest::REQUEST_READY);
		request_body.clear(); return false;
	}
	request.writeChunkTofile(request_body.substr(0, chunk_size));							// write data to temp file
	request_body = request_body.substr(chunk_size + 2); // +2 EXPECTING CRLF AFTER CHUNK
	return (true);
}


bool	ServerDrive::getBody(HttpRequest &request) {

	const std::string	&length_str = request.getHeaderValue("Content-Length");
	std::string			&request_body = request.getRequestData();// main buffer 
	size_t				content_length =  std::atoi(length_str.c_str());
	size_t				bytes_left;

	bytes_left = content_length - request.getRequestBodySize();
	if (bytes_left > 0 && request_body.size() <= bytes_left) {
		//const std::sting &chunk = request_body.substr(0, bytes_left);
		request.writeChunkTofile(request_body);
		request_body = "";
 		//request.appendChunk(request_body.substr(0, bytes_left));
	}
	if(request.getRequestBodySize() == content_length) 
		return (true);
	return (false);
}

void ServerDrive::CheckRequestStatus(Client &client) {
	HttpRequest &client_request = client.getRequest();

	if (client_request.getRequestState() == HttpRequest::HEADER_STATE) {
		getHeader(client_request);
	}
	if (client_request.getRequestState() == HttpRequest::BODY_STATE)  {

		if (client_request.getRequestData().size() > client.getServer().getClientMaxBodySize())
				throw(RequestError(ErrorNumbers::_413_PAYLOAD_TOO_LARGE));
		client_request.openDataFile();
		if (client_request.getBodyTransferType() == HttpRequest::CHUNKED) {
			while (!client_request.getRequestData().empty() && unchunkBody(client_request)) {
			}
		}
		else if (client_request.getBodyTransferType() == HttpRequest::CONTENT_LENGHT) {
			if (getBody(client_request))  {
				client_request.setRequestState(HttpRequest::REQUEST_READY);
			}
		}
		else
			throw(RequestError(ErrorNumbers::_411_LENGTH_REQUIRED));
	}
	if (client_request.getRequestState() == HttpRequest::REQUEST_READY)  {
		FD_SET(client.getConnectionFd() , &(this->_writeset)); 
		const Server & client_server = getServerByName(client_request.getHeaderValue("Host")); // Supports virtual hosting 
		client.setServer(client_server);
		#if DEBUG 
		ConsoleLog::Debug("server handeling the request : " + client.getServer().getServerName());
		#endif
	}
}

void ServerDrive::CloseConnection(int fd) {
	Network::closeConnection(fd);
	FD_CLR(fd, &this->_readset);
	FD_CLR(fd, &this->_writeset);
	this->_server_fds.erase(std::find(this->_server_fds.begin(), this->_server_fds.end(), fd));
	this->_clients.erase(fd);
}

void ServerDrive::checkClientTimout(int fd) {

	Client &client				= getClient(fd) ;
	time_t last_event			= client.getClientRequestTimeout();
	time_t config_timeout 		= this->_client_timeout; // PS: READ HEADER 
	time_t elapsed				= time (NULL) - last_event;

	if (elapsed >= config_timeout) {
		ConsoleLog::Warning("Connection Timout Closing ... ");
		throw(RequestError(ErrorNumbers::_408_REQUEST_TIMEOUT));
	}
}

char *moveToHeap(const std::string &resp) {
	const char *r = resp.c_str();
	char *resp_copy = new char [resp.size()];
	memmove(resp_copy, r, resp.size());
	return (resp_copy);
}

void PrepareResponse(Client &client)  {

	Handler	handler(client);
	
	// shit's dirty i konw , IAM SO TIRED !
	std::string location_checked = "";
	int status = handler.getStatus();
	
	if (handler.getReturnUrl().length() > 0){
		location_checked = handler.getReturnUrl();
		status = handler.getReturnStatus();
	} else {
		location_checked = handler.getLocation();
	}
	
	Response response(handler.getBody(), handler.getType(), handler.getCookie(),location_checked, handler.getSize(), status, handler.getFD(), handler.getPath(), handler.getErrorPages());
	client.setResponseObj(response);
}

void ServerDrive::SendResponse(Client &client) {
	Response &client_response	= client.getResponseObj();
	short 	client_fd			= client.getConnectionFd();
	size_t	socket_buffer_size	= Network::getSocketBufferSize(client_fd, SO_RCVBUF);
	const std::string &response	= client_response.getResponseChunk(socket_buffer_size);
	size_t	response_size		= response.size();
	bool	close_connection	= false;
		
	if (response_size == 0) 
			close_connection = true;
	if (int ss = send(client_fd, response.c_str(), response_size, 0) != (ssize_t ) response_size) {
			close_connection = true;
		#if DEBUG
		client.getRequest().setStatusCode(500);//deadbeef
		ConsoleLog::Warning("Send Error: failed to  writre data to socket !");
		#endif 
	}
	if (close_connection) {
		#if DEBUG
		std::string debug_log = "::WebServ Response Sent!. Closing fd :...";
		debug_log = debug_log + std::to_string(client_fd);
		ConsoleLog::Debug(debug_log);
		#endif 
		log(client);
		CloseConnection(client_fd);
	}
}

void ServerDrive::log(Client &client) {
	if (client.getRequest().getStatusCode()) {
		ConsoleLog::Error("[Send Error]");
		return ;
	}
	Response 	&response_obj = client.getResponseObj();
	const std::string log = "[Response]: " + client.getRequest().getRequestLine() + " (" +  response_obj.getStatusCode() + ")";
	ConsoleLog::Specs(log);
}

void ServerDrive::eventHandler(fd_set &read_copy, fd_set &write_copy) {

	int fd_max = this->_fd_max;

	for (int fd = 3; fd <=  fd_max; fd++) {
		try { 
			if (FD_ISSET(fd, &write_copy)) {
				Client &client = getClient(fd);
				if (not client.ResponseReady())
					PrepareResponse(client);
				#if DEBUG
				ConsoleLog::Debug("Sending response ...");
				#endif 
				SendResponse(client);
			}
			else if (FD_ISSET(fd, &read_copy)) {
				if (FD_ISSET(fd, &this->_listenset)) 
					addClient(Network::acceptConnection(fd));	// new connection 
				else 											// read request 
					readRequest(fd);
			}
			else if (!FD_ISSET(fd, &this->_listenset) 
					&& FD_ISSET(fd, &(this->_readset)) 
					&& !FD_ISSET(fd, &this->_writeset)) // CLIENT SHOULD BE CHECKED FOR TIMEOUT
				checkClientTimout(fd);	

		} catch (const RequestError &error) {
			#if DEBUG
			ConsoleLog::Error("Request Error: status code : " + std::to_string(error.getErrorNumber()));
			#endif
			FD_CLR(fd, &(this->_readset));
			FD_SET(fd, &this->_writeset);		// select before response
			getClient(fd).setRequestStatus(error.getErrorNumber());
		}
	}
}

void ServerDrive::run() {
	fd_set read_copy;
	fd_set write_copy;

	while (1) {
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
	else {
		throw(ErrorLog("BUG: Potential Server error"));
	}
}

const Server &ServerDrive::getServerByName(const std::string &host_name) {
	typedef std::vector<Server>::const_iterator cv_iterator;

	for (cv_iterator it = this->_virtual_servers.begin(); it != this->_virtual_servers.end(); it++) {
		if (it->getServerName() == host_name) 
			return (*it);
	}
	return (*this->_virtual_servers.begin());
}

const Server &ServerDrive::getServerByFd(int fd) {
	typedef std::vector<Server>::const_iterator cv_iterator;

	for (cv_iterator server = this->_virtual_servers.begin(); server != this->_virtual_servers.end(); server++) {
		if (server->getSocketFd() == fd) 
			return (*server);
	}
	return (*this->_virtual_servers.begin());
}

