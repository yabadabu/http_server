# http server
Super basic http server in c++ with no extra dependencies.

Just a simple c++ class to serve HTTP GET requests in a port. No thread is created by this class, but with the interface you can poll for activity before timeout.

No argument parsing or fancy checkings. But it's super easy to return data from a mobile app
back to a PC.

Uses miniz library to compress answers to the client (taken from https://github.com/richgel999/miniz).

# Demo VisualStudio

Open VisualStudio (2015/2017). Compile the solution on folder visualc and run it.
Open a browser and enter http://localhost:8080
The browser will request two files, an html and a bunch of png images.

# Demo OSX

```bash
cd osx
make
make run
```
Open a browser and enter http://localhost:8080
The browser will request two files, an html and the png image.

# Usage
 
Derive your own class from HTTP::CBaseServer, and implement the method: 

```c++
  bool onClientRequest(const TRequest& r, TSocket client) override;
```

Do whatever you need and write the server results in the client socket. This method is called from the thread which called the server.tick or server.runForEver()

You probably want to do our own stuff and check for activity periodically. The argument
in the tick method is the amount of time (in usecs) to wait before returning. 0 will wait nothing

```c++
  CMyServer server;
  if (!server.open(8080))
    return -1;
  do {
    // .. your other code
    server.tick(0);
    // .. your other code
  }
```

