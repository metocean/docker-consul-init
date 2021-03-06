# docker-consul-init
A INIT / PID 1 program that starts your app and a consul agent written in C.

- [docker-consul-init](#docker-consul-init)
  * [usage](#usage)
  * [example](#example)
  * [on docker stop / SIGTERM](#on-docker-stop)
  * [on docker kill -s SIGNAL](#on-docker-kill-signal)
  * [docker signals](#docker-signals)
  * [c make](#c-make)
  * [Dockerfile example](#dockerfile-example)

## usage
```
  usage: consul-init --map [from-sig] [to-sig] --init [program / args ..] --program [program / args ..]

 --map [from-sig] [to-sig]: this re-maps a signal received by consul-init app to the program, you can have more than one mapping

 --program [norm program args]: this is the program + it args to be run in the docker

 --init [init program args]: the init program runs first, before consul and --program. If it returns nonzero consul-init will exit. 

 --no-consul: do not use the consul agent

 example: consul-init --map TERM QUIT --program /bin/nginx -g daemon off;
 example: consul-init --map TERM QUIT --init wget http://[somesite]/config.json --program /bin/nginx -g daemon off;
 
 consul agent is started with:

 /usr/bin/consul agent -config-dir /consul/config -data-dir /consul/data
 
 Note these consul directories must exist or the consul agent will not start.

```

## example
this will start nginx and the consul agent. When ```docker stop``` is used nginx will gractefully shutdown and consul will deregister the service.
```
/bin/consul-init --map TERM QUIT --program /bin/nginx -g daemon off;
```
```--map``` maps the terminate signal to quit allowing nginx to gracefully shut down.
```
/bin/consul-init --map TERM QUIT --init wget http://[somesite]/config.json --program /bin/nginx -g daemon off;
```
```--init``` will run wget before the nginx or consul-agent. If wget exits none zero consul-init will exit without running consul agent or the program nginx.

## on docker stop
on docker stop / SIGTERM or on SIGINT consul-init will:
1. stop the consul agent greacefully allowing it to redregister itself.
2. send a SIGTERM to the program, or if the user has mapped TERM to another signal it will send the mapped [to-signal].

## on docker kill SIGNAL
on docker kill -s SIGNAL consul-init will send the SIGNAL to the program, or if the user has mapped SIGNAL to another signal it will send the mapped [to-signal].

## docker signals
* ```docker stop```: The main process inside the container will receive SIGTERM, and after a grace period (default 10 seconds), SIGKILL.
* ```docker kill -s SIGNAL```: will send a singal to the process in the container.

## c make
```
cd consul-init
make
make clean
```

## Dockerfile example
```
# install consul agent
ENV CONSUL_VERSION=1.0.6
RUN cd /tmp &&\
    curl -o consul.zip -L https://releases.hashicorp.com/consul/${CONSUL_VERSION}/consul_${CONSUL_VERSION}_linux_amd64.zip &&\
    unzip consul.zip &&\
    chmod +x consul &&\
    mv consul /usr/bin &&\
    mkdir /consul &&\
    rm -rf /tmp/*

# install consul-init
ENV CONSUL_INIT_VERSION=0.0.8
RUN echo "----------------- install consul-init -----------------" &&\
    cd /tmp &&\
    curl -o consul-init.tar.gz -L https://github.com/metocean/docker-consul-init/archive/v${CONSUL_INIT_VERSION}.tar.gz &&\
    tar -vxf consul-init.tar.gz &&\
    cd /tmp/docker-consul-init-${CONSUL_INIT_VERSION}/consul-init &&\
    make &&\
    cp consul-init /bin/ &&\
    mkdir -p /consul/data &&\
    rm -rf /tmp/consul-init

ENTRYPOINT ["consul-init", "--program", "gunicorn", "mywebapp", "--bind", "0.0.0.0:80"]
```
