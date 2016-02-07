# http server
Super basic http server in c++

Just a simple c++ class to serve HTTP requests in a port. No thread is created by this class, but with the interface you can poll
for activity before timeout.

# Demo

Open VC2015. Compile the solution on folder vc2015 and run it.
Open a browser and enter http://localhost:8080
The browser will request two files, an html and the png image.

# Usage
 
Derive your own class from HTTP::CBaseServer, and implement the method: 

```c++
  bool onClientRequest(const TRequest& r, TSocket client) override;
```

Do whatever you need and write the server results in the client socket. This method is called from the thread which called the server.tick

The server must open some port and then you can runForEver or poll for activity (with timeout) 

```c++
  CMyServer server;
  if (!server.open(8080))
    return -1;
  server.runForEver();
```
