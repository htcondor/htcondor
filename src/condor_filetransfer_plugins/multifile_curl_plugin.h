#ifdef WIN32
#define CURL_STATICLIB // this has to match the way the curl library was built.
#endif

#include <curl/curl.h>

using namespace std;

struct transfer_request {
    string local_file_name;
};

static FileTransferStats* _global_ft_stats;

class MultiFileCurlPlugin {

  public:

    MultiFileCurlPlugin( int diagnostic );
    ~MultiFileCurlPlugin();

    int InitializeCurl();
    int DownloadFile( const char* url, const char* local_file_name );
    int DownloadMultipleFiles( string input_filename );
    int ServerSupportsResume( const char* url );
    static size_t HeaderCallback( char* buffer, size_t size, size_t nitems );
    static size_t FtpWriteCallback( void* buffer, size_t size, size_t nmemb, void* stream );
    void InitializeStats( string request_url );

    CURL* GetHandle() { return _handle; };
    string GetStats() { return _all_files_stats; }

  private:

    CURL* _handle;
    FileTransferStats* _this_file_stats;
    int _diagnostic;
    string _all_files_stats;

};