# UPD-server
This is a UPD Server-Client program on C

## Deployment 
`git clone https://github.com/ahmetugsuz/UPD-server.git`

### Running/Usage

From the root directory: `UDP-server` run `make upush_client` and then `./upush_client <PORT> <LOSS_PROBABILITY>`. You choose the PORT number, Example PORT number: `2222`.

Open a new terminal to make a client, then `make upush_client` from the root directory, make the client by, 
`./upush_client <NICK> <ADDRESS> <PORT> <TIMEOUT> <LOSS_PROBABILITY>`, 
Notice: that you need to connect with the same PORT as the server, and Address is the IP-address who is available, example IP: `127.0.0.1`
Make several clients (from new terminal windows).

### Communication
To send message to other client('s) type:

`@<NICK> <MSG>`

### Example 
The Server: `./upush_server 2222 0`

Client 1: `./upush_client ola 127.0.0.1 2222 100`
Client 2: `./upush_client frida 127.0.0.1 2222 1000 0`.

#### From client 1 to client 2
@frida hey.

@frida sup?

#### client 2 responding
`ola: hey`

`ola sup?`

@ola Heeey

@ola nothing, U?

#### client 1 received answer
`frida: Heeey`

`frida: nothing, U?`

@frida miss u 



