# Goals
- Automate development process on any consoles
- Follow web standards (http/websocket), so no fancy client-side apps are required
- Complient with any web client (VitaWebView, Chrome, lynx, w3m, curl)
- Easy customisable UI (see style.css)
- Allow web sites interaction (ie: VPK install)

### `/file/` Management

#### List directory content

```sh
curl $VITAIP/file/ux0:/
```

#### Upload a `file.txt` to `ux0:/`

```sh
curl $VITAIP/file/ux0:/ -File=@file.txt
```

#### Show a File

```sh
curl $VITAIP/file/ux0:/id.dat
```

#### Download+unzip on the fly

```sh
curl $VITAIP/file/ux0:/data/psp2core-my.psp2dmp | gunzip > dump
```

#### Remove all `*.psp2dmp` files from `ux0:/data/`:

```sh
for f in $(curl $VITAIP/file/ux0:/data/ | grep psp2dmp); do
	curl $VITAIP/file/ux0:/data/ -do=Unlink -dfile=$f;
done
```


### `/module/` Management

#### List loaded modules :

```sh
curl $VITAIP/module/
```

#### Find a module name by PID, Path...:

```sh
curl $VITAIP/module/ | grep $PID | awk '{print $4}'
```

#### Load + Start a module:

```
PID=$(curl $VITAIP/module/ -do=LoadStart -dpath=ux0:my.suprx -dargs=hello)
```

This will Load `ux0:my.suprx` and start it with `hello` as `_start()`/`main()` argument
The LoadStart value (pid) will be stored so we can StopUnload it later.

#### Stop + Unload a module:

```sh
curl $VITAIP/module/ -do=StopUnload -dmodid=$PID -dargs=bye
```
This will call the `module_stop()` function with `bye` as argument, then unload the module.
 
#### Eboot suprx file

Eboots suprx are more tricky to load than plugins suprx :
- They will hold the module loader starting call until they `return`/`exit()`ed.
- If they tries to `return`/`exit()`, they crt0 will call `sceKernelExitProcess()`
- `sceKernelExitProcess()` wich kill every processes, including the loader (our server).

In order to start an eboot suprx we must:
- Use `sceKernelExitDeleteThread(0)` instead of return/exit
- If you have an infinite-loop eboot : create a killswitch to break the loop
- Load the eboot asynchronously (don't wait for it PID to be returned)

```sh
# Compile and upload our eboot (named net_http_sample.self here, yes I know it's confusing)
cmake .. && make && curl $VITAIP/file/ux0: -File=@net_http_sample.self
# Asynchrounously (`&`) Load+Start it with args and store it PID in a `pid` file
curl -s $VITAIP/module/ -do=LoadStart -dpath=ux0:net_http_sample.self -dargs=8080 > pid &
# Insert your test / wait / screenfetch here
sleep 5
# Call the children killswitch (because loaded app is a while(1) kind)
curl -L $VITAIP:8080/exit/
# Now that the eboot is terminated, we can StopUnload it using it freshly returned pid
curl $VITAIP/module/ -do=StopUnload -dmodid=$(cat pid)
```

We can even reload our app on source change:

```sh
set -x
while inotifywait -se close_write ../src/*.c; do
cmake ..>/dev/null && make    >/dev/null && curl $VITAIP/file/ux0: -File=@net_http_sample.self &&
bash -c "curl -s $VITAIP/module/ -do=LoadStart -dpath=ux0:net_http_sample.self -dargs=8080 > pid &" && sleep 1 &&
curl -s $VITAIP:8080/camera/0.jpg &&
curl -s $VITAIP/screen/go.jpg > screen.jpg &&
curl -L $VITAIP:8080/exit/ 2> /dev/null &&
curl -s $VITAIP/module/ -do=StopUnload -dmodid=$(cat pid)
done
```

### `/camera/` access

#### Camera MJPEG Stream

```
xdg-open http://$VITAIP/camera/1.mjpg;
```

#### Get camera image (like IPcam)

```
curl $VITAIP/camera/1 > cam.jpg;
```

#### Timelapse

```
watch -n60 curl $VITAIP/camera/1 > $(date -Iseconds).jpg
cat *.jpg | ffmpeg -f image2pipe -i - timelapse.mkv
```

example: i.imgur.com/klcB8If.gif

### `/screen/` capture

#### Display the screen in the terminal*

```
w3m $VITAIP/screen/
```

*require `w3m-img` package

#### MJPEG Stream

Simply use a fake `.mjpg` filename to get the screen stream as MJPEG.

```
xdg-open $VITAIP/screen/foo.mjpg
```

### `/ctrl/` buttons

#### map X11 mouse on the left PSVita stick

```
curl $VITAIP/ctrl/mode -dvalue=1 #enable sticks sampling
curl $VITAIP/ctrl/divider -dvalue=2 # decrease returned value ([-128,+128] range become [-32,+32])
curl $VITAIP/ctrl/threshold -dvalue=4 # set the L/R stick deadzone
curl -sN http://192.168.1.100/ctrl/0 | while read ctrl; do # read the ctrl stream
	xdotool mousemove_relative -- ${ctrl:10:9}; # move mouse using left stick value
done
```


### Continous Integration example

The following snipet build, upload and reload your binary on the console
when any of it sources files changes.

```sh
while inotifywait -se close_write ../src/*.c; do
	cmake .. && make &&
	curl -sFile=@plugin.suprx $VITAIP/file/ux0:/ &&
	ID=$(curl $VITAIP/module/ -do=LoadStart -dpath=ux0:plugin.suprx -dargs=12345") &&
	curl $VITAIP/module/ -do=StopUnload -dmodid=$ID;
done
```

## Create your own RESTful Micro service
Easy to remember, easy to use from your host terminal.

The `utils.c` functions allow to easily create new microservices.
Microservices are modular so you can create/disable any service according to your needs.
