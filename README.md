# FTP-Server

## Compiling and running

To compile, do `make`.

To run the server, do `cd bin` and then `./server.out`. When running the server, please ensure the current working directory is `bin` (the directory with the binaries); the server assumes this is the case so that it reads the necessary files in `bin/server`.

The server control port and data port are `2100` and `2000` by default. To change them, modify the constants in `common.h.`. They are not `21` and `20` by default because, in this case, the server would require `sudo` privileges to run and bind to them. Although we may have `sudo` privileges on our local machines, we do not have them on the NYUAD Linux server, which is why we had to change the ports to `2100` and `2000.`

To run the client, you can do `cd bin` and then `./client.out`. However, the client may be run from anywhere on the system.

## Testing

Some things we did to test that our FTP server and client work:

* Testing expected inputs and verifying output as in the example provided in the assignment document; ensuring server response codes follow RFC specifications

```
220 Service ready for new user.
ftp> USER steve
331 Username OK, need password.
ftp> PASS muffins
230 User logged in, proceed.
ftp> CWD test
200 directory changed to /Users/steve/test
ftp> RETR chocolate_muffin.txt
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> LIST
200 PORT command successful.
150 File status okay; about to open data connection.
chocolate_muffin.txt
red_velvet_cookie.txt
226 Transfer completed.
ftp> QUIT 
221 Service closing control connection.
```

* Executing commands requiring authentication without having logged in first

```
220 Service ready for new user.
ftp> !CWD clients
Directory changed to /home/vlatko/projects/FTP-Server/bin/clients
ftp> STOR a.txt
532 Need account for storing files.
ftp> RETR b.txt
532 Need account for storing files.
ftp> QUIT
221 Service closing control connection.
```

* Attempting to log in with incorrect credentials

```
220 Service ready for new user.
ftp> USER wrongusername
530 Not logged in.
ftp> PASS nousername
503 Bad sequence of commands.
ftp> USER steve
331 Username OK, need password.
ftp> PASS wrongpassword
530 Not logged in.
ftp> USER steve
331 Username OK, need password.
ftp> PASS muffins
230 User logged in, proceed.
ftp> USER bob
503 Bad sequence of commands.
ftp> QUIT
221 Service closing control connection.
```

* Storing and retrieving files of different types of sizes, including .txt, .jpg, .exe. .zip, etc. Sample files from our testing are stored in `bin/clients`.

```
220 Service ready for new user.
ftp> USER bob
331 Username OK, need password.
ftp> PASS donuts 
230 User logged in, proceed.
ftp> !CWD clients
Directory changed to /home/vlatko/projects/FTP-Server/bin/clients
ftp> LIST 
200 PORT command successful.
150 File status okay; about to open data connection.
folder
b.txt
a.txt
226 Transfer completed.
ftp> CWD folder
200 directory changed to /Users/bob/folder
ftp> STOR a.txt
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> STOR exe.exe
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> STOR jpg.jpg
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> STOR png.PNG
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> STOR txt.txt
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> STOR zip.zip
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> LIST     
200 PORT command successful.
150 File status okay; about to open data connection.
png.PNG
txt.txt
exe.exe
a.txt
zip.zip
jpg.jpg
226 Transfer completed.
ftp> QUIT 
221 Service closing control connection.
```

```
220 Service ready for new user.
ftp> USER bob
331 Username OK, need password.
ftp> PASS donuts
230 User logged in, proceed.
ftp> LIST
200 PORT command successful.
150 File status okay; about to open data connection.
folder
b.txt
a.txt
226 Transfer completed.
ftp> !CWD clients
Directory changed to /home/vlatko/projects/FTP-Server/bin/clients
ftp> !LIST
png.PNG
retrieving
txt.txt
exe.exe
a.txt
zip.zip
jpg.jpg
ftp> RETR b.txt
200 PORT command successful.
150 File status okay; about to open data connection.
226 Transfer completed.
ftp> QUIT
221 Service closing control connection.
```

* Client attempting to move outside of their server user directory, e.g. by sending `CWD ..` or `CWD ../(username of other user)`

```
220 Service ready for new user.
ftp> USER bob
331 Username OK, need password.
ftp> PASS donuts
230 User logged in, proceed.
ftp> PWD
257 /Users/bob
ftp> CWD ..
550 No such file or directory.
ftp> CWD ../steve
550 No such file or directory.
ftp> CWD ../bob   
200 directory changed to /Users/bob
ftp> QUIT
221 Service closing control connection.
```

* Client attempting to retrieve a file outside of their user storage directory 

```
220 Service ready for new user.
ftp> USER bob
331 Username OK, need password.
ftp> PASS donuts
230 User logged in, proceed.
ftp> RETR ../steve/test/chocolate_muffin.txt
200 PORT command successful.
550 Requested action not taken. File name not allowed.
ftp> QUIT
221 Service closing control connection.
```

* Listing files in server directory and on local client directory, including empty directories

```
220 Service ready for new user.
ftp> USER bob
331 Username OK, need password.
ftp> PASS donuts
230 User logged in, proceed.
ftp> LIST
200 PORT command successful.
150 File status okay; about to open data connection.
folder
b.txt
a.txt
empty_folder
226 Transfer completed.
ftp> CWD empty_folder
200 directory changed to /Users/bob/empty_folder
ftp> LIST
200 PORT command successful.
150 File status okay; about to open data connection.

226 Transfer completed.
ftp> QUIT
221 Service closing control connection.
```

Some other things we tested more broadly:

* Connecting with multiple clients to validate supporting multiple simultaneous clients

* Connecting with multiple clients to the same user

* Verifying server correctly imports user data from `bin/server/users.txt`, by logging into each user

* Connecting to the server via `telnet` and ensuring nothing breaks on the server
