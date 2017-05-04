# docker-consul-init
A INIT / PID 1 program that starts your app and a consul agent written in C.
  
## Dockerfile Entry Point
```
ENTRYPOINT ["/bin/consul-init", "--program", "/myapp", "--my-app-arggs"]
```

## usage
```
usage: consul-init --map [from-signal] [to-signal] --program [program-path] [program args ..]
--map [from-signal] [to-signal]: this re-maps a signal received by consul-init app to the program
--program [norm program args]: this is the program + it args to be run in the docker
```

## docker signals
* docker stop: The main process inside the container will receive SIGTERM, and after a grace period (default 10 seconds), SIGKILL.
* docker kill -s SIGNAL: will send a singal to the process in the container.

## on docker stop / SIGTERM:
on docker stop / SIGTERM consul-init will:
1. stop the consul agent greacefully allowing it to redregister itself.
2. send a SIGTERM to the program, or if the user has mapped TERM to another signal it will send the mapped [to-signal].

## on docker kill -s SIGNAL:
on docker kill -s SIGNAL consul-init will send the SIGNAL to the program, or if the user has mapped SIGNAL to another signal it will send the mapped [to-signal].

## nginx example
```
/bin/consul-init --map TERM OUIT --program /bin/nginx -g daemon off;
```
```--map``` maps the terminate signal to quit, which means nginx will gracefully shut down.
