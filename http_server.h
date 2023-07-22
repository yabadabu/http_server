#ifndef INC_HTTP_SERVER_H_
#define INC_HTTP_SERVER_H_

#if defined( _WIN32 )

#include <WinSock2.h>
typedef SOCKET TSocket;
typedef int socklen_t;
#else

#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
typedef int    TSocket;
#define closesocket  close

#endif

#include <vector>
#include <sys/types.h> 
#include <ctime>
#include <string>

namespace HTTP {

// -------------------------------------------------------
struct VBytes : public std::vector<char> {
  bool send(TSocket fd) const;
  bool recv(TSocket fd);
  void format(const char* fmt, ...);
  bool read(const char* file);
};

class CBaseServer {
  
  class VSockets : public std::vector<TSocket> {
  public:
    void remove(TSocket s);
  };

public:

  // -------------------------------------------------------
  struct TRequest {

    struct THeaderLine {
      const char* title;    // 'User-Agent'
      const char* value;    // 'curl/7.53.0'
    };

    // only supporting GET
    enum eMethod { GET, UNSUPPORTED };
    eMethod     method;

    // / or /index.html...
    std::string url;
    
    // Save header lines
    static const int max_header_lines = 64;
    THeaderLine lines[max_header_lines];
    int         nlines = 0;
    const char* getHeader( const char* title ) const;
    bool headerContains(const char* title, const char* text_in_header) const;
    std::string getURIParam( const char* title ) const;
    std::string getURLPath() const;

    bool parse(VBytes& buf, bool trace);

    // Who has generated the request
    TSocket     client;
  };

private:

  // -------------------------------------------------------
  struct TActivity {
    fd_set   fds;
    VSockets ready_to_read;
    bool wait(VSockets& sockets, unsigned timeout_usecs);
  };
  
  // -------------------------------------------------------
  bool    createServer(int port);
  TSocket acceptNewClient();
  void    prepare();

  // -------------------------
  TSocket   server;
  VSockets  active_sockets;
  VBytes    inbuf;
  TActivity activity;

protected:
  
  void sendAnswer( 
      const TRequest&   r
    , const VBytes& answer_data
    , const char* content_type
    , const char* content_encoding = nullptr    // gzip, deflate, br or null
    );

  // Will try to compress your answer automatically if the client suppots compression
  void compressAndSendAnswer( 
      const TRequest&   r
    , const VBytes& answer_data
    , const char* content_type
    );

public:

  virtual bool onClientRequest(const TRequest& r) = 0;
  virtual ~CBaseServer();

  bool open(int port);
  void close();

  // This will block for timeout_usecs at most. 0 just to poll
  bool tick(unsigned timeout_usecs);
  void runForEver();
  bool trace = false;
};

}

#endif
