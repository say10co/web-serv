

#ifndef HANDLER_CPP
#define HANDLER_CPP

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include "../Server/Client.hpp"

class Handler {

    Client client;

    
    std::string body;
    std::string type;
    int status;
    int size;

    int fd;
	std::string file_path;


    public:
        Handler(Client &client);
        //Handler(std::string path, std::string req_body, std::map<std::string, std::string> headers);

        std::string getFileContent(std::string filename);
        int getFileContentLength(std::string filename);
        std::string getFileContentType(std::string filename);

        int getSize();
		const std::string &getBody();
        std::string getType();
        int getStatus();
        int getFD();
		const std::string &getFilePaht() const;

		~Handler() {

		}
};

#endif
