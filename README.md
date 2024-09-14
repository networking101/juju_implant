# juju_implant
implant written in C


## install cmocka

https://cmocka.org/
```
cd cmocka-1.1.7
mkdir build
cd build
cmake ..
make
```



### TODO
add message type for diagnostic messages  
create landing function for each thread and use global variable to close all threads on sigint  
put file  
get file  
handle more than FDSIZE agents  
encrypted comms  