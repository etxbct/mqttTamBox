# mqttTamBox
Is a tool to do TAM (TågAnMälan) between neighboring stations.

TAM Box is used:

	on sending side: allocated a track between two stations and request to send a train.
	on receiving side: accept/reject incomming request.

The TAM Box has in total four exits, A-D.
The left side has A and C and the right side has B and D.
Left and right track are set from the outgoing track view. Single track is always named left track.

	             Station Front side
	    +----------------------------------+
	----| A  right track     left track  B |----
	----| A  left track     right track  B |----
	    |                                  |
	    |                                  |
	----| C  left track                  D |
	    +----------------------------------+
	             Station Back side

## Topics and payloads
The syntax of topics and payloads are based on Richard Hughes mqtt-lcp with some adaptation.

## Message Types
There are two basic types of messages that can be used: "command" and "data".

### Data messages

The "data" type messages are used to report information from one node to other nodes concerning the state of "something".
The data messages are intended to be one-to-many.

The "something" could be a signal: "signal block-1 is now set to approach". Or it could be a turnout: "turnout steel-mill has been closed".

The "something" could also be a "ping" message proclaiming that the node is alive and running well.

Data type message topics always begin with the prefix "dt":

- "dt / `<scale>` / `<type>` / `<node-id>` / `<port_id>`" - data message

## Command messages

The other broad class of messages are "commands". These messages are intended to be one-to-one type messages. One node requests another node perform an operation. "Set signal block-1 to approach". The subscribing node processes the command request and publishes a response. "Signal 123 has been set to approach".

>Since MQTT topics are being used to control message flow, the requesting node does not really know which actual node will respond to the command request. Any physical node could have subscribed to the topic in question.

Command message topics begin with the prefix of "cmd". Whether a "command" message is a request or response is indicated by the last term of the topic:

- "cmd / `<scale>` / `<type>` / `<node-id>` / `<port_id>` / req" - command request
- "cmd / `<scale>` / `<type>` / `<node-id>` / `<port_id>` / res" - command response

>Response? Why do nodes need to send a response?
Please remember that MQTT is a distributed message system. The node making a request does not directly communicate with the node that will perform the operation. All messaging is indirect via a broker computer. Without a response mechanism, the requestor does not know if its request was even received let alone successfully carried out.

>MQTT does have a built in mechanism to ensure messages are delivered. It is called Quality if Service (QOS). It is a mechanism handled in the low level MQTT client library code. Depending which client library you are using you may see different levels of support for QOS. The lowest level of QOS, level 0 is fine to use for MQTT-LCP.

### Session ID

Two particular parts of the request body are very import in the request/response pair. First, the request body must contain a session id that requestor uses to match responses to outstanding request. A requestor may have many outstanding requests at one time. Imaging a throttle requesting a certain route. If there are six turnouts in the route, the throttle may issue six different switch requests. Having the same session id in the response that was in the request allows the throttle to know which switches were changed by matching responses to requests.

### Respond To

The other important part of the request is the "respond-to". It is used by the responder as the topic of the response it publishes to the requestor. In snail-mail terms, it is the return address or in email it is the “from”.

It is recommended that the response topic be the in this form:

Topic: `cmd/h0/node/node-id`

To receive the responses, the requesting node would subscribe to:

Subscribe: `cmd/h0/node/<node-id>/#`

Thereafter no matter what kind of request (tam, node) was made by the requestor node, the responses could be received at one point in app.

### Used cmd by mqttTamBox

cmd are used when requesting to send a train.

When single track is used between stations then track is always set to left.

When double track is used then the requested track should be set to the receiving station track so if, as in Sweden, left track is used to send traffic the train is departing on left track but arriving on right track (see picture above).

### Topics
Topic used:

	message  /scale  /type  /node-id   /port-id  /order  payload
	cmd      /h0     /tam   /tambox-4  /a        /req    {json}        TAM request message
	cmd      /h0     /tam   /tambox-4  /b        /res    {json}        TAM response message
	dt       /h0     /tam   /tambox-4  /b                {json}        TAM data message
	dt       /h0     /ping  /tambox-4                    {json}        Ping message

	message : message sent, cmd (command) or dt (data).
	scale   : distinguish different scales from each other when the same broker is used.
	type    : type of message.
	node-id : the id of the MQTT client.
	port-id : exit/entry letter of the station, A-D.
	order   : command message order, req (request) or res (response).


Type can be any of these:

	tam     : TAM message
	ping    : ping message, sent every 10 second
	node    : node message


Order can be any of these:

	req     : a request
	res     : a response to a request

### Payloads
MQTT message topics are used to request to send a train to next station.
The MQTT message body, on the other hand, contains the contents of the message.

The body is a JSON formatted message body. Each message body's JSON conforms to a format suggested by Richard Hughes mqtt-lcp project.

There are some common elements in all JSON message bodies:
- root-element: Type of message ("tam", "ping", "node"), matches topic type in the message topic.
- "version": version number of the JSON format
- "timestamp": time message was sent in seconds since the unix epoch (real time, not fastclock)
- "session-id": a unique identifier for the message.

>Exception: The session-id in a response must match the session-id of the corresponding request message.

- "node-id": The application node id to receive the message. Matches node-id in message topics.
- "port-id": The port on the receiving node to which the message is to be applied. Matches port-id in the message topic
- "identity": Optional. The identification of a specific item like a loco or rfid tag.
- "respond-to": " Message topic to be used in the response to a command request. The "return address".
- "state": The state being requested to be changed or the current state being reported.
	- State has two sub elements:
		- desired: The state to which the port is to be changed: "accept", "in", etc. Used in command type messages
		- reported: The current state of a port: "accepted", "rejected", etc. Used command response and data messages
- "metadata"   : Optional. Varies by message type.


### Example of messages

Traffic direction change to tambox-4 on right track:

Topic: `cmd/h0/tam/tambox-4/a/req`
Body: `{"tam": {"version": "1.0", "timestamp": 1707767518, "session-id": "req:1707767518", "node-id": "tambox-1", "port-id": "a", "track": "right", "respond-to": "cmd/h0/tam/tambox-1/b/res", "state": {"desired": "in"}}}`
 
Traffic direction change to tambox-4 on right track accepted by tambox-4:

Topic: `cmd/h0/tam/tambox-1/b/res`                    
Body: `{"tam": {"version": "1.0", "timestamp": 1707767534, "session-id": "req:1707767518", "node-id": "tambox-4", "port-id": "b", "track": "right", "state": {"desired": "in", "reported": "in"}}}`

Request train 2123 to tambox-2 on right track:

Topic: `cmd/h0/tam/tambox-2/a/req`
Body: `{"tam": {"version": "1.0", "timestamp": 1707768634, "session-id": "req:1707768634", "node-id": "tambox-1", "port-id": "a", "track": "right", "identity": 2123, "respond-to": "cmd/h0/tam/tambox-1/a/res", "state": {"desired": "accept"}}}`

Train 2123 to tambox-2 accepted by tambox-2:

Topic: `cmd/h0/tam/tambox-1/a/res`
Body: `{"tam": {"version": "1.0", "timestamp": 1707768655, "session-id": "req:1707768634", "node-id": "tambox-2", "port-id": "a", "track": "right", "identity": 2123, "state": {"desired": "accept", "reported": "accepted"}}}`

Train 348 to tambox-5 rejected on right track:

Topic: `cmd/h0/tam/tambox-1/a/res`
Body: `{"tam": {"version": "1.0", "timestamp": 1707768656, "node-id": "tambox-5", "port-id": "a", "track": "left", "identity": 348, "session-id": "req:1707768634", "state": {"desired": "accept", "reported": "rejected"}}}`

Train 348 to tambox-5 was canceled before tambox-5 send accepted or rejected:

Topic: `cmd/h0/tam/tambox-5/a/req`
Body: `{"tam": {"version": "1.0", "timestamp": 1707768766, "node-id": "tambox-5", "port-id": "a", "track": "right", "identity": 348, "session-id": "req:1707768766", "respond-to": "cmd/h0/tam/tambox-1/a/res", "state": {"desired": "cancel"}}}`

Train 348 reported out on track left by tambox-5:

Topic: `dt/h0/tam/tambox-5/a`
Body: `{"tam": {"version": "1.0", "timestamp": 1707768766, "node-id": "tambox-5", "port-id": "a", "track": "left", "identity": 348, "state": {"reported": "out"}}}`

Train 348 reported in on track right by tambox-1:

Topic: `dt/h0/tam/tambox-1/a`
Body: `{"tam": {"version": "1.0", "timestamp": 1707769346, "node-id": "tambox-1", "port-id": "a", "track": "right", "identity": 348, "state": {"reported": "in"}}}`

Tambox-1 sending ping:

Topic: `dt/h0/ping/tambox-1`
Body: `{"ping": {"version": "1.0", "timestamp": 1707768845, "node-id": "tambox-1", "state": {"reported": "ping"}, "metadata": {......}}}`

Example of metadata:
`"metadata": {"type": "mqttTamBox", "ver": "ver 2.0.0", "name": "Charlottendahl", "sign": "CDA", "rssi": "-65 dbm"}`