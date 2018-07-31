# CTRL+ESC

CTRL+ESC is a simple Linux daemon that grabs keyboard input and emulates the following:

* When CTRL is held, it behaves the same way it always does - as a modifier.

* When CTRL key is pressed and released, ESC key press is generated.

This way, it's possible to keep both key functions on the same physical key.

Besides that, some mac-ishms are implemented (that way separate keys for
Home/End/PageUp/PageDown are not required):

* CTRL+Left arrow simulate Home.

* CTRL+Right arrow simulate End.

* CTRL+Up simulate Page Up.

* CTRL+Down simulate Page Down.

F8 can be used to shutdown the daemon in case something went horribly wrong.

## Similar projects

* [XCape](https://github.com/alols/xcape) - where it all started, X11 only.

* [caps2esc](https://gitlab.com/interception/linux/plugins/caps2esc) - more flexible system, but more complicated and requires some configuration.

* [evcape](https://github.com/wbolster/evcape) - written in Python, not SUID-able, requires configuration.

## Why another one?

Since all these projects (this one included) are essentially the key loggers,
my intention here is to keep the code small, readable and audit-able.
There is no user configuration and only a hardcoded set of functionality.

## How does it work?

It grabs the keyboard input device (meaning that no other application gets
the events from that device) and forwards/modifies the events to the
`/dev/uinput`.

## Installation

```
git clone https://github.com/fomichev/ctrlesc.git
cd ctrlesc
sudo apt-get install libevdev-dev
make
```

Find the device you want to use:

```
$ ls -la /dev/input/by-id/
total 0
drwxr-xr-x 2 root root 120 Jul 16 08:06 .
drwxr-xr-x 4 root root 440 Jul 30 16:49 ..
lrwxrwxrwx 1 root root   9 Jul 16 08:06 usb-05f3_0007-event-if01 -> ../event4
lrwxrwxrwx 1 root root   9 Jul 16 08:06 usb-05f3_0007-event-kbd -> ../event3
lrwxrwxrwx 1 root root   9 Jul 10 15:56 usb-Kingsis_Peripherals_Evoluent_VerticalMouse_4-event-mouse -> ../event2
lrwxrwxrwx 1 root root   9 Jul 10 15:56 usb-Kingsis_Peripherals_Evoluent_VerticalMouse_4-mouse -> ../mouse0
```

Make the binary suidable (both `/dev/input/eventX` and `/dev/uinput` require
root because they are machine-wide):

```
sudo chown root:$GROUPS ctrlesc
sudo chmod u+s ctrlesc
```

Prepare systemd service config:

```
$ cat ~/.config/systemd/user/ctrlesc.service
[Unit]
Description=ctrlesc

[Service]
ExecStart=/absolute/path/to/ctrlesc /dev/input/by-id/<keyboard device>
Restart=always

[Install]
WantedBy=default.target
```

Start it:

```
systemctl --user start ctrlesc.service

# if everything works, enable it upon restart with
systemctl --user enable ctrlesc.service
```
