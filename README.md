# mqttTamBox
Is a tool to do TAM (TågAnMälan) between neighboring stations.


With the mqttTamBox a request is sent to next station to ask for permission to send a specific train by typing in the Schedule number for the train.
The receiving station can accept or reject the request.  
If it is accepted the seding station report when the train is leaving the station.  
The receiving station report when the train arrive at the station.  

## Procedures
### Sending side
1. Start  
   ![image](https://github.com/user-attachments/assets/e9922b66-561c-427b-84c3-f8b8a85ab3a3)
2. To send a train to LEK press A  
   ![image](https://github.com/user-attachments/assets/974282e3-e948-4089-b38d-f1eae6eb80cf)
3. Type the Scheduled train number and press '#'  
   ![image](https://github.com/user-attachments/assets/22ed1863-754f-4e36-a4b7-292821cc0dc2)  
4. A ? symbol is visible during the time the other station haven't answered the request and the stations short name is replaced with the train number  
   ![image](https://github.com/user-attachments/assets/58bf2234-0064-47c6-9cc2-a39284060001)
5. A text is shown if the train was accepted or rejected  
   ![image](https://github.com/user-attachments/assets/46590f5b-079e-447c-b1c2-bf41cea398fa)  
   If accepted the ? symbol is changed to an outging symbol, on left side '<', on right side '>'  
   ![image](https://github.com/user-attachments/assets/b030754b-fef6-4517-9e08-475c698a0603)
6. When the train is leaving report departure by pressing the same letter pushed in step 2 and then push '#'  
   ![image](https://github.com/user-attachments/assets/66d289a3-449d-41b0-954c-670b45bd7abe)  
   ![image](https://github.com/user-attachments/assets/c0969e36-2d06-44e6-9bcc-1a5bd050baa5)  
   the outgoing symbol is changed to filled Arrow with same direction as the outgoing symbol
7. A text is shown when the other station report arrival of the train  
   ![image](https://github.com/user-attachments/assets/d88cfc20-fccb-4390-8197-8f4e9b45c62a)  
   the symbol is  changed to '<' or '>' and the train number is replaced with the station short name.  
   ![image](https://github.com/user-attachments/assets/cc2261c6-511d-41e4-a473-a861364117b5)
### Receiving side
1. Start  
   ![image](https://github.com/user-attachments/assets/940713ce-a937-483a-ba5b-98cd1ada6fce)
2. A request arrives  
   ![image](https://github.com/user-attachments/assets/8f3ffdac-44b3-416f-998f-d73ba04bc8d9)
3. Accept or reject by pressing '#' to accept or '*' to reject
4. When the request has been accepted  
   ![image](https://github.com/user-attachments/assets/553e9a9a-48c5-4755-9357-e8e5168d103b)
5. When the other station report departure of the train  
   ![image](https://github.com/user-attachments/assets/588beeaa-ea07-4bf2-857f-64088a36d2a7)  
   the incomming symbol is changed to filled Arrow with same direction as the incomming symbol
   ![image](https://github.com/user-attachments/assets/d2778894-013a-4182-8e81-04a3b7c05393)
6. When the train arrives report arrival by pressing the same letter pushed in step 2 and then push '#'  
   ![image](https://github.com/user-attachments/assets/1d3f418a-3854-4ca6-bc7f-a38ace001996)  
   the symbol is  changed to '<' or '>' and the train number is replaced with the station short name.  
   ![image](https://github.com/user-attachments/assets/940713ce-a937-483a-ba5b-98cd1ada6fce)
