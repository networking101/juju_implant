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
add message type for diagnostic messages  
handle more than FDSIZE agents  
encrypted comms  
handle newlines from shell response fragments  
figure out consistent return variables in functions (ret_val)  
handle queue mutex. We need to mutex to lock until agent_send sends all fragments of message  
when the agent is force closed, the console hangs  
