# docker-consul-init
A INIT / PID 1 program that starts your app and a consul agent written in C.

## usage
```
usage:

consul-init --map [from-signal] [to-signal] --program [program-path] [program args ..]

--map [from-signal] [to-signal]: this re-maps a signal received by consul-init app to the program, you can have more than one mapping
--program [norm program args]: this is the program + it args to be run in the docker
--no-consul: do not use the consul agent

consul agent is started with:

/usr/bin/consul agent -config-dir /etc/consul -data-dir /var/lib/consul/data

Note these consul directories must exist or the consul agent will not start.
```

## on docker stop / SIGTERM:
on docker stop / SIGTERM consul-init will:
1. stop the consul agent greacefully allowing it to redregister itself.
2. send a SIGTERM to the program, or if the user has mapped TERM to another signal it will send the mapped [to-signal].

## on docker kill -s SIGNAL:
on docker kill -s SIGNAL consul-init will send the SIGNAL to the program, or if the user has mapped SIGNAL to another signal it will send the mapped [to-signal].

## docker signals
* docker stop: The main process inside the container will receive SIGTERM, and after a grace period (default 10 seconds), SIGKILL.
* docker kill -s SIGNAL: will send a singal to the process in the container.

## nginx example
```
/bin/consul-init --map TERM OUIT --program /bin/nginx -g daemon off;
```
```--map``` maps the terminate signal to quit, which means nginx will gracefully shut down.

## make
```
cd consul-init
make
```

## Dockerfile example:
```
# install consul agent
ENV CONSUL_VERSION=0.7.5
RUN cd /tmp &&\
    curl -o consul.zip -L https://releases.hashicorp.com/consul/${CONSUL_VERSION}/consul_${CONSUL_VERSION}_linux_amd64.zip &&\
    unzip consul.zip &&\
    chmod +x consul &&\
    mv consul /usr/bin &&\
    mkdir /consul &&\
    rm -rf /tmp/*

# install consul-init
ADD ./consul-init /tmp/consul-init
RUN cd /tmp/consul-init &&\
    make &&\
    cp consul-init /bin/ &&\
    mkdir -p /var/lib/consul/data &&\
    rm -rf /tmp/consul-init

ENTRYPOINT ["consul-init", "--program", "gunicorn", "mywebapp", "--bind", "0.0.0.0:80"]
```
