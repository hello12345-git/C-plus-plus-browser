## Install Dependencies

```
sudo apt-get update
sudo apt-get install -y libgtkmm-3.0-dev libwebkit2gtk-4.0-dev xvfb
```
## Compile & Run 

### 1.

```
g++ -std=c++17 -O2 -o browser_gui browser.cpp \
  $(pkg-config --cflags --libs gtkmm-3.0 webkit2gtk-4.0)
```

### 2.

```
xvfb-run -a ./browser_gui
```
