# mqttTamBox
Is a tool to do TAM (TågAnMälan) between neighboring stations.

TAM Box is used:

	on sending side   : allocated a track between two stations and request to send a train.
	on receiving side : accept/reject incomming request.

The TAM Box has in total four exits, A-D.
The left side has A and C and the right side has B and D.
Left and right track are set from the outgoing track view. Single track is always named left track.

	    +----------------------------------+
	----| A  right track     left track  B |----
	----| A  left track     right track  B |----
	    |                                  |
	    |                                  |
	----| C  left track                  D |
	    +----------------------------------+

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

	type        root    /client   /dest /report type /track  /item       /order
	Traffic   - mqtt_h0 /tambox-4 /cda  /traffic     /left   /direction  /state
	Traffic   - mqtt_h0 /tambox-2 /sns  /traffic     /left   /train      /accept
	Traffic   - mqtt_h0 /tambox-1 /b    /traffic     /right  /train      /request

	root       : name of MQTT root.
	client     : name of the MQTT client.
	dest       : receiving station id.
	report type: only traffic supported.
	track      : left or right.
	item       : name of the reporter.
	order      : reported order.

Order can be one of these:

	request    : request to send a train
	accept     : accept the request to send a train
	cancel     : cancel the request to send a train
	reject     : reject the request to send a train
	in         : report train received at station
	out        : report train leaving station
	state      : set state of track direction

### Payloads
Payloads for traffic:

	direction  : {in,out}
	train      : id (schedule number for the train)

### Examples of mqtt messages

	Topic                                              Payload  Comment
	mqtt_h0/tambox-4/cda/traffic/left/direction/state  out      Traffic direction to CDA on left track is out
	mqtt_h0/tambox-2/gla/traffic/left/train/request    2123     Request outgoing train 2123 to GLA on left track
	mqtt_h0/tambox-2/sal/traffic/left/train/accept     2123     Train 2123 to SAL accepted on left track
	mqtt_h0/tambox-1/vst/traffic/right/train/reject    348      Train 348 to VST rejected on right track
	mqtt_h0/tambox-1/vst/traffic/right/train/cancel    348      Train 348 to VST was canceled before VST send accept or reject
 
