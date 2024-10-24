# juju_implant
implant written in C

## dependencies
sudo apt-get install gcc-multilib

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
handle more than FDSIZE agents  
encrypted comms  
handle newlines from shell response fragments  
when the agent is force closed, the console hangs  
change base.c to utility_listener.c  
handle all errors (malloc, calloc)  
udp  
daemonize  
Make sure buffer read from recv is not greater than fragment size  
add colors to different message types  