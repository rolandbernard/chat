Chat
======

This is a simple chat server and client. Use `chat --help` for some information.

Here is the output of `chat --help`:
```
Usage: chat [options]

Options:
  -h, --ip IP            set the servers ip (def: '127.0.0.1')
  -p, --port PORT        select the servers port (def: '24242')
  -s, --server           make this a server
  -H, --auto-discovery   use automatic discovery

Options for clients:
  -n, --name NAME        set the name (def: username)
  -G, --no-group         do not use the group feature
  -B, --ignore-break     do not worry about breaking words
  -U, --no-utf-8         avoid any utf-8 I/O processing
  -g, --group GROUP      set the group (def: 'default')
  -a, --alternet        *use the alternet frame buffer
  -t, --typing-info      send typing info
  -L, --no-log-info      don't send enter and exit info
  -k, --key KEY          encrypt mesages with the given key
  --help                 show this help page

* This may cause problems if the terminal
  does not support certain features
```

---
###### Copyright (c) 2019 Roland Bernard
