#ifndef DEFAULTCONFIG_HPP
# define DEFAULTCONFIG_HPP 

#include <iostream>
#define REQUIRED -1

class DefaulConfig
{
	public:
	   	static const std::string ERROR_LOG;
	   	static const std::string ROOT;
		static const std::string INDEX;
		static const bool		 AUTOINDEX;
	   	static const size_t 	 CLIENT_MAX_BODY_SIZE;
	   	static const size_t 	 CLIENT_REQUEST_TIMOUT;
		static const std::string ADDRESS; 
		static const short		 PORT;
	   	static const std::string SERVER_NAME;
		static const std::string UPLOAD_STORE;
		static const bool		ALLOW_UPLOADS;
};

#endif /* DEFAULTCONFIG_HPP */
