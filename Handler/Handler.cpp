

#include "Handler.hpp"
#include "CGI.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include "utils.h"
#include "../ConfigParse/inc/location.hpp"

Handler::Handler(){ }

Handler::Handler(Client &client){
    
    this->client = client;
	this->fd = -1;
    HttpRequest request = client.getRequest();
    this->rootPath = this->client.getServer().getRoot() + "/";
    this->errorPages = this->client.getServer().getErroPages();
    std::vector<Location> locations = client.getServer().getLocations();
    std::string upload_location = client.getServer().getUploadStore();

    int autoindex = client.getServer().getAutoIndex();
    std::string index = client.getServer().getIndex();
    std::string request_path = request.getRequestPath();


    if(this->client.getServer().getReturnURL().length() > 0){
        this->return_url = this->client.getServer().getReturnURL();
        this->return_status = this->client.getServer().getReturnCode();
        this->body = "";
        this->size = 0;
        this->type = "";
        this->fd = -1;
        return;
    }

    // check location 
    int locationIndex = findMatchingLocation(request_path, locations);
    
    if(locationIndex > -1){
       
        index = locations[locationIndex].getIndex();

        // overlaped request !
        request_path = removeEndpoint(request_path, locations[locationIndex].getEndpoint());
        
        //request.setRequestPath(new_request_path);

        
        
        if(locations[locationIndex].getReturnURL().length() > 0){
            this->return_url = locations[locationIndex].getReturnURL();
            this->return_status = locations[locationIndex].getReturnCode();
            this->body = "";
            this->size = 0;
            this->type = "";
            this->fd = -1;
            return;
        }else {
            this->return_url = "";
            this->return_status = -1;
        }

        if(locations[locationIndex].getUploadStore().size() > 0){
            upload_location = locations[locationIndex].getUploadStore();
        }
       
        // check allowed methods !
        if(!checkAllowedMethods(locations[locationIndex], request.getRequestMethod())){
            this->body = "<html><body style='text-align: center'><h1>405 Method Not Allowed</h1></body></html>";
            this->size = 85;
            this->status = 405;
            this->type = "text/html";
            return;
        }
    }

   

    if(request.getStatusCode() > 0){
        std::string res = generateErrorResponse(request.getStatusCode());
        this->body = res;
        this->size = res.size();
        this->type = "text/html";
        this->status = request.getStatusCode();
        return;
    }



	// joined location path with resource name 
	std::string path = rootPath + request_path;
    
    std::string method = request.getRequestMethod();
    std::map<std::string, std::string> req_headers = request.getHeaders();
    if((method == "POST" || method == "DELETE") && !isCGIScript(path)){
        // handle POST / DELETE
        int status = -1;

        if(method == "POST" && upload_location.length() > 0){
            std::string filepath = rootPath + upload_location + getFilenameFromRequestPath(request_path);
            
            if(filepath.length() == 0){
                this->body = "<html><body style='text-align:center'><h1>404 Not Found</h1><h3>webserv</h3></body></html>";
                this->type = "text/html";
                this->size = 91;
                this->status = 404;
                return;
            }
            std::cout << "try to upload !\n";
            status = write_file(filepath, request.getRequestDataFilename());
        }else{
            
            std::string filepath = rootPath + request_path;
            
            if(!fileExists(filepath)){
                this->body = "<html><body style='text-align:center'><h1>404 Not Found</h1><h3>webserv</h3></body></html>";
                this->type = "text/html";
                this->size = 91;
                this->status = 404;
                return;
            }

            status = delete_file(filepath);
            if(status == 403){
                this->body = "<html><body style='text-align:center'><h1>403 Unautorized</h1><h3>webserv</h3></body></html>";
                this->type = "text/html";
                this->size = 93;
                this->status = 403;
                return;
            }
        }

        // check if any of the methods above failed ! 
        if(!status){
            this->status = 500;
            this->body = "<html><body style='text-align:center'><h1>500 Internal Server error</h1><h3>webserv</h3></body></html>";
            this->type = "text/html";
            this->size = 103;
        }else if(status){
            this->status = 201;
            this->body = "<html><body style='text-align:center'><h1>201 Created</h1><h3>webserv</h3></body></html>";
            this->type = "text/html";
            this->size = 89;
        }

        return;
    }
    else if(!fileExists(path) && !directoryExits(path)){
        
        this->body = "<html><body style='text-align:center'><h1>404 Not Found</h1><h3>webserv</h3></body></html>";
        this->type = "text/html";
        this->size = 91;
        this->status = 404;

        return;
    }else if (directoryExits(path)){
        if(!autoindex){
            path = rootPath + index;
            if(!fileExists(path)){
                this->body = "<html><body style='text-align:center'><h1>404 Not Found</h1><h3>webserv</h3></body></html>";
                this->type = "text/html";
                this->size = 91;
                this->status = 404;
                return;
            }
        }else {          
            std::string autoIndexResponse = ListFile(path);
            
            this->body = autoIndexResponse;
            this->status = 200;
            this->type = "text/html";
            this->size = autoIndexResponse.size();
            
            return;
        }
    }
    
    if(isCGIScript(path)){
        // handle cgi !
        CGI cgi(client);
		//std::cout << "php cgi ok" << std::endl;
        cgi.handlePhpCGI(path);
        std::string response = cgi.getResponse();
        std::map <std::string, std::string> parsed_cgi_response = cgi.parse_cgi_response(response);
       
        this->body = parsed_cgi_response["body"];
        this->type = parsed_cgi_response["type"];
        this->size = parsed_cgi_response["body"].size();
		this->location = parsed_cgi_response["redirect"];
        this->status = 200;
		if (this->location != "") {
			this->cookie = parsed_cgi_response["cookie"];
        	this->status = 303;
		}
    
    }else{
        //this->body = this->getFileContent(path);
        this->fd = getFileFd(path);
		this->type = this->getFileContentType(path);
		this->size = this->getFileContentLength(path);
        this->status = 200;
    
    }
}


std::string Handler::getFileContent(std::string filename){
    std::ifstream file(filename, std::ios::binary);
    if (!file || isDirectory(filename)) {
		std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    char* buffer = new char[file_size];
    file.read(buffer, file_size);
    file.close();

	std::string file_contents(buffer, file_size);
    delete[] buffer;
    return file_contents;
}

int Handler::getFileContentLength(std::string filename){
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
		std::cerr << "Failed to open file: " << filename << std::endl;
		return 0;
    }

    // Determine the file size
    file.seekg(0, std::ios::end);
    int file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    file.close();

	return file_size;
}

std::string Handler::getFileContentType(std::string filename){

	std::string file_type = "application/octet-stream"; 
	size_t      dot_pos = filename.find_last_of(".");
	
    if (dot_pos != std::string::npos) {
		std::string extension = filename.substr(dot_pos + 1);
		if (extension == "html") {
			file_type = "text/html";
		} else if (extension == "css") {
			file_type = "text/css";
		} else if (extension == "js") {
			file_type = "text/javascript";
		} else if (extension == "png") {
			file_type = "image/png";
		} else if (extension == "jpg" || extension == "jpeg") {
			file_type = "image/jpeg";
		} else if (extension == "mp4"){
            file_type = "video/mp4";
        } else if (extension == "mp3"){
            file_type = "audio/mpeg";
        } else if (extension == "pdf"){
            file_type = "application/pdf";
        }
	}
	return file_type;
}

int Handler::getSize(){
    return this->size;
}

const std::string &Handler::getBody(){
    return this->body;
}

std::string Handler::getType(){
    return this->type;
}

int Handler::getStatus(){
    return this->status;
}

int Handler::getFD(){
    return this->fd;
}

