# docker-consul-init
A INIT / PID 1 program that starts your app and a consul agent written in C.
  
In Dockerfile
```ENTRYPOINT ["/bin/consul-init", "/myapp", "--my-app-arggs"]
