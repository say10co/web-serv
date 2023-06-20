
#include "Response.hpp"
#include <iomanip>
#include <iostream>
#include <ctime>
#include <fstream>
#include <unistd.h>

/*
 *
 * HELPER fucntion that get current date and time in the format required by http !
 *
 */
std::string Response::response_chunk = "";
std::string Response::body = "";
//std::ifstream Response::static_file();

std::string nowHTTP(){
 	// Get the current time
    std::time_t current_time = std::time(nullptr);

    // Convert to a struct tm in UTC
    std::tm* gm_time = std::gmtime(&current_time);

    // Format the time according to HTTP-date format
    char http_date[30];
    std::strftime(http_date, sizeof(http_date), "%a, %d %b %Y %H:%M:%S GMT", gm_time);

	return http_date;
}
/*
 * Adding response copy constructor (adriouic)
 * */
Response::Response(const Response &response) {
	*this = response;
}
Response & Response::operator=(const Response &response) {

	if (this == &response) 
		return (*this);
	this->contentType = response.contentType;
	this->contentLength = response.contentLength;
	this->date = response.date;
	this->server = response.server;
	this->connection = response.connection;
	this->cacheControl = response.cacheControl;
	this->accept = response.accept;
	this->acceptEncoding = response.acceptEncoding;
	this->host = response.host;
	this->status = response.status;
	this->status_message = response.status_message;
	this->fd = response.fd;
	this->body = response.body;
	this->headears_sent =  response.headears_sent;
	this->last_offset = response.last_offset;
	return (*this);
}

Response::Response(std::string body, std::string type, int size, int status, int fd){
	this->fd = fd;
	this->last_offset = 0;
	this->status = std::to_string(status);
	this->status_message = generateStatusMessage(status);
	this->contentType = type;
	this->contentLength = std::to_string(size);
	// add content length 
	this->date = nowHTTP();
	this->server = "webserv/1.0 (macos)";
	this->connection = "Keep-Alive";
	this->cacheControl = "no-cache, no-store, must-revalidate";
	this->accept = "*/*";
	this->acceptEncoding = "gzip, deflate, sdch";
	this->host = "localhost:89";
	this->body = body;
	this->headears_sent = false;
}

std::string Response::generateResponse(){

	std::string response = "";

	// set header 
	response += "HTTP/1.1 " + this->status +  " " + this->status_message + "\r\n";
	response += "Content-Type: " + this->contentType + "\r\n";
	response += "Content-Length: " + this->contentLength + "\r\n";
	response += "Cache-Control: " + this->cacheControl + "\r\n";
	response += "Date: " +  this->date  + "\r\n";
	response += "Server: " + this->server + "\n";
	response += "Connection: " + this->connection + "\r\n";
	response += "Accept: " + this->accept + "\r\n";
	response += "Accept-Encoding: " + this->acceptEncoding + "\r\n";
	response += "Host: " + this->host + "\r\n\r\n";

	response += this->body + "\r\n";

	return response;
}

std::string Response::getResponseHeaders(){
	std::string headers = "";

	headers += "HTTP/1.1 " + this->status +  " " + this->status_message + "\r\n";
	headers += "Content-Type: " + this->contentType + "\r\n";
	headers += "Content-Length: " + this->contentLength + "\r\n";
	headers += "Cache-Control: " + this->cacheControl + "\r\n";
	headers += "Date: " +  this->date  + "\r\n";
	headers += "Server: " + this->server + "\n";
	headers += "Connection: " + this->connection + "\r\n";
	headers += "Accept: " + this->accept + "\r\n";
	headers += "Accept-Encoding: " + this->acceptEncoding + "\r\n";
	headers += "Host: " + this->host + "\r\n\r\n";

	return headers;
}

const std::string &Response::getResponseBody(){
	this->body = this->body + "\r\n";

	return this->body;
}

const std::string &Response::getResponseChunk(int size){

   //std::string response = "";
   std::string &response = this->response_chunk;

	if (! this->headears_sent) {
		this->headears_sent = true;
		response = getResponseHeaders();
		return (response);
	}
    if(this->fd == -1) {
		response = this->body;
		this->body.clear();
        return response;
	}

    char buffer[size + 1];

    int status = read(this->fd, buffer, (size_t)size);
    if(status > 0){
        response = std::string(buffer, status);
    }else {
		std::cout << "this->fd " << this->fd << std::endl;
        perror("read failed : ");
    }
    return response;
}

std::string Response::getContentLength(){
	return this->contentLength;
}


std::string Response::generateStatusMessage(int status){
	if(status == 404)
		return "Not Found";
	else if (status == 500)
		return "Internal Server Error";
	else if (status == 403)
		return "Forbidden";
	else if (status == 401)
		return "Unauthorized";
	else if (status == 503)
		return "Service Unavailable"; 
	else if (status == 405)
		return "Method Not Allowed";
	switch  (status) { 
		case (400): return   "Bad request";
		case (408): return   "Request timeout";
		case (411): return   "Length required";
		case (414): return   "Uri_too long";
		case (413): return   "Payload too large";
		case (431): return   "Request header fields too large";
		case (500): return   "Internal server error";
		case (501): return   "No timplemented";
		case (505): return   "Http version not supported";
		default:
			return "OK";
	}
}


int Response::getFD(){
	return this->fd;
}

const std::string &Response::getStatusCode() const  {
	return (this->status);
}
