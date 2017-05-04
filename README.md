# docker-consul-init
A INIT / PID 1 program that starts your app and a consul agent written in C.

## make
```
cd consul-init
make
cp consul-init /bin/consul-init
```

## Dockerfile entry point
```
ENTRYPOINT ["/bin/consul-init", "--program", "/myapp", "--my-app-arggs"]
```

## usage
```
usage:

consul-init --map [from-signal] [to-signal] --program [program-path] [program args ..]

--map [from-signal] [to-signal]: this re-maps a signal received by consul-init app to the program, you can have more than one mapping
--program [norm program args]: this is the program + it args to be run in the docker
--no-consul: do not use the consul agent

consul agent is started with:

/usr/bin/consul agent -config-dir /etc/consul -data-dir /var/lib/consul/data

Note these consul directories must exist.
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
