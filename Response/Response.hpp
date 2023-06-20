#ifndef RESPONSE
#define RESPONSE

#include <string>
#include <unistd.h>
class Response {

	std::string contentType;
	std::string contentLength;
	std::string date;
	std::string server;
	std::string connection;
	std::string cacheControl;
	std::string accept;
	std::string acceptEncoding;
	std::string host;
	std::string status;
	std::string status_message;

	int fd;
	bool	headears_sent;
	size_t	last_offset;
	static std::ifstream static_file;

	static std::string response_chunk;
	static std::string body;

	public:
		Response(std::string body, std::string type, int size, int status, int fd);
		~Response()  {  } 
		std::string generateResponse();
		
		const std::string &getResponseBody();
		std::string getResponseHeaders();
		
		std::string getContentLength();
		std::string generateStatusMessage(int status);
		const std::string &getResponseChunk(int size);

		int getFD();
		Response(const Response &response);
		Response & operator=(const Response &response);
		Response() {};
		const std::string &getStatusCode() const;


};


#endif // !RESPONSE
