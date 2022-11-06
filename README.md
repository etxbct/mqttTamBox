# mqttTamBox
Is a tool to do TAM (TågAnMälan) between neighboring stations.

TAM Box is used:

	on sending side   : allocated a track between two stations and request to send a train.
	on receiving side : accept/reject incomming request.

The TAM Box has in total four exits, A-D.
The left side has A and C and the right side has B and D.

In Sweden when double track is used one track is the up track (uppspår) and the other track is down track (nedspår).
If facing the station from backside the nearest outgoing track is the down track (nedspår) carrying traffic from right to left.
The furthest outgoing track is the up track (uppspår) carrying traffic from left to right.
If single outgoing track is used then the track is the up track (uppspår).

For train direction *up* is when trains are going in default traffic direction.
For train Direction *down* is when trains are going in oposite direction.
So for a single track; if train is going from left to right it is having the traffic direction *up*.

### LEDs
Each track direction is using key A-D. A and C is train direction to the left, and B and D is train direction to the right.
Each A-D button has a RGB led which can show:

	green      : train direction is outgoing
	red        : train direction is incoming
	flash green: outgoing request sent
	flash red  : incoming request received
	yellow     : outgoing/incoming request accepted

### Topics
Topic used:

	type        root    /client   /exit /report type    /track  /item       /order
	Traffic   - mqtt_n  /tambox-4 /a    /traffic        /up     /direction  /state
	Traffic   - mqtt_n  /tambox-2 /b    /traffic        /up     /train      /id
	Traffic   - mqtt_n  /tambox-1 /c    /traffic        /down   /train      /request

	root       : name of MQTT root.
	client     : name of the MQTT client.
	report type: type of reporter.
	track      : up or down.
	item       : name of the reporter.
	order      : reported order.

Order can be one of these:

	request	   : request to send a train
	accept     : accept the request to send a train
	reject     : reject the request to send a train
	state      : set state of track direction
	id         : id (schedule number for the train)

### Payloads
Payloads for traffic:

	Direction  : {up,down}
	Train      : id (schedule number for the train)

### Examples of mqtt messages

	Topic                                         Payload  Comment
	mqtt_n/tambox-4/b/traffic/up/direction/state  up       Traffic direction on up track is up ( going to right) on exit B
	mqtt_n/tambox-2/a/traffic/up/train/request    2123     Request outgoing train 2123 on up track on exit A
	mqtt_n/tambox-2/a/traffic/up/train/accept     2123     Train 2123 accepted on up track on exit A
	mqtt_n/tambox-4/c/traffic/down/train/reject   342      Train 348 rejected on down track on exit C
