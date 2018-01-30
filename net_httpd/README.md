# Goals
- Automates development process on any PS consoles
- Use standards protocols (http/websocket)
- Extensible API: other webapp can interact with it (ex: for VPK install)

# What is offer

## POSIX compliant
The Source shall compile and work on any libc compliant plateform

## Basic HTML interface

Work on any browser from FF and Chrome to lynx and w3m.
Go to http://0.0.0.0 from your vita to access it.
No javascript needed.

## Customisable UI
Modify your style.css to your needs

## RESTful Micro service architecture
Easy to remember, easy to use from your host terminal.

The `utils.c` functions allow to easily create new microservices.
Microservices are modular so you can create/disable any service according to your needs.

### `/file/` Management

Browse a directory

```
curl $IP/file/ux0:/
```

Upload a `file.txt` to `ux0:/`

```
curl $IP/file/ux0:/ -F data=@file.txt
```

Show a File

```
curl $IP/file/ux0:/id.dat
```

Download+unzip on the fly

```
curl $IP/file/ux0:/data/psp2core-my.psp2dmp | gunzip > dump
```

Remove all `*.psp2dmp` files from `ux0:/data/`:

```
for f in $(curl $IP/file/ux0:/data/ | grep psp2dmp); do
	curl $IP/file/ux0:/data/ -daction=Unlink -dfile=$f;
done
```


### `/module/` Management

List loaded modules : 

```
curl $IP/module/
```

Find a module by name, PID, Path:

```
curl $IP/module/ | grep $PID | awk '{print $4}'
```

Load `ux0:/my.suprx` module and start it with `hello` as `_start()` argument:

```
PID=$(curl $IP/module/ -daction=LoadStart -dpath=ux0:my.suprx -dargs=hello)
```

This will store the started module PID in the $PID variable.
You can then, stop it with `bye` as argument and unload it using:

```
curl $IP/module/ -daction=StopUnload -dmodid=$PID -dargs=bye
```

Deploy and reload a second HTTP Server

```
cmake .. && make && curl $IP/file/ux0: -F data=@net_http_sample.self
curl -s $IP/module/ -daction=LoadStart -dpath=ux0:net_http_sample.self -dargs=8080 > pid &
curl -L $IP:8080/exit/
curl $IP/module/ -daction=StopUnload -dmodid=$(cat pid)
```

### `/camera/` access

Get camera image (like IPcam)

```
//TODO
```

Do a timelapse

```
//TODO
```

### `/screen/` capture

Display the screen in the terminal (need w3m-img package)

```
w3m $IP/screen/
```

### Continous Integration example

The following snipet rebuild and loadStart your binary on the as soon as you changed any of your souces files.

```
IP=192.168.1.100
cd bin
while inotifywait -se close_write ../src/*.c; do
	cmake .. && make &&
	curl -sF data=@plugin.suprx $IP/file/ux0:/ &&
	ID=$(curl $IP/module/ -daction=LoadStart -dpath=ux0:plugin.suprx -dargs=12345") && 
	curl $IP/module/ -daction=StopUnload -dmodid=$ID; 
done
```


## CURL file upload dump
```
Host: 0.0.0.0:8080
Content-Length: 6331 << size of all form-data headers + file content
Expect: 100-continue << Only in curl
Content-Type: multipart/form-data; boundary=------------------------3367985a15c2414c

--------------------------3367985a15c2414c
Content-Disposition: form-data; name="data"; filename="Makefile"
Content-Type: application/octet-stream

[The file]
--------------------------3367985a15c2414c--
```
