# universal-remote-control-skill

This repository contains resources related to the Universal Remote Control Alexa Skill

# High-Level Description of the Solution

The system consists of the following components:
* Global components:
  * Alexa Smart Home Skill
  * AWS Lambda function
  * AWS API Gateway websocket endpoint (backed by additional AWS Lambda functions to handle connections)
* Per user-components:
  * Google Account containing a Google Spreadsheet with a fixed name
  * Universal Remote Controller (URC) device based on an ESP8266 micro-controller

The Alexa Smart Home Skill is linked to the user's Google Account in order to access the spreadsheet.
When the user requests device discovery, the skill passes the discovery request on to the AWS Lambda function 
which in turn reads the rows from the spreadsheet and translates them into Alexa Smart Home Device metadata.
Each row contains
* URCID (a unique ID of the URC device derived from an on-board DS2401 chip)
* user-friendly name of the appliance to control, e.g. "Ceiling Fan"
* device category (as prescribed by Alexa Smart Home API)
* operation to perform on that device, e.g. TurnOn/TurnOff
* channel of communication with the appliance (infrared of 433MHz radio waves)
* data package to transmit to the appliance to effect the intended operation

The URC device upon startup creates a websocket connection to the AWS API Gateway via WiFi and passes on its unique URCID.
The AWS Lambda function handling websocket connections stores the combination of URCID and websocket connection ID in an
AWS Dynamo DB table.

When the user requests a device operation via voice command, the skill sends another request to the AWS Lambda function
which refers to the device metadata created during discovery. The AWS Lambda function uses the URCID to find an active
websocket connection ID in the AWS Dynamo DB table, deleting any stale connection IDs it finds. It then sends a message
to the URC device via that websocket connection. This message contains the chosen channel and data package from the Google spreadsheet.

The URC device listens on the open websocket connection for messages and transmits the data to the appliance via infrared of 433MHz radio waves.

![Experimental assembly of the URC device](https://github.com/reisners/universal-remote-control-skill/blob/master/esp8266/image1.jpg =400x)

