#include <rss_core_log.hpp>
#include <rss_core_error.hpp>
#include <rss_core_server.hpp>

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv){
	int ret = ERROR_SUCCESS;
	
	if (argc <= 1) {
		printf(RTMP_SIG_RSS_NAME" "RTMP_SIG_RSS_VERSION
			"Usage: %s <listen_port>\n" 
			RTMP_SIG_RSS_URL"\n"
			"Email: "RTMP_SIG_RSS_EMAIL"\n",
			argv[0]);
		exit(1);
	}
	
	int listen_port = ::atoi(argv[1]);
	rss_trace("listen_port=%d", listen_port);
	
	RssServer server;
	
	if ((ret = server.initialize()) != ERROR_SUCCESS) {
		return ret;
	}
	
	if ((ret = server.listen(listen_port)) != ERROR_SUCCESS) {
		return ret;
	}
	
	if ((ret = server.cycle()) != ERROR_SUCCESS) {
		return ret;
	}
	
    return 0;
}