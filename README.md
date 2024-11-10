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
udp  
add colors to different message types  
check if functions that aren't exported have static keyword  
issue with removing agents from poll (break agent with gdb then list agent when removed)  