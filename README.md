# mqttTamBox
Is a tool to do TAM (TågAnMälan) between neighboring stations.


With the mqttTamBox a request is sent to next station to ask for permission to send a specific train by typing in the Schedule number for the train.
The receiving station can accept or reject the request.  
If it is accepted the seding station report when the train departures the station.  
The receiving station report when the train arrives at the station.  

The software has only been tested on a node MCU V3 (esp8266).

### Features
* Request to send a train.
* Cancel a request.
* Accept or reject a request.
* Automatic time out of a request if not answered (time configurable on internal configuration page).
* Report departure.
* Report arrival.
* Keep a live ping sent every 10 second.
* Remotely restart of a mqttTamBox.
* Remotely shutdown of a mqttTamBox.
* Support of different LCD sizes (size configurable on internal configuration page).
* Global configuration fetched from configuration server (server configurable on internal configuration page).
* Firmware update from internal configuration page.

Check the [wiki](https://github.com/etxbct/mqttTamBox/wiki) for more detailed information.
