# 2. Overall System Architecture

Date: 2019-04-21

## Status

Accepted

## Context

To put down an architecture for the system as a whole.

## Decision

The system consists of the following components:
* Global components:
  * Alexa Skill
  * AWS Lambda
* Per user-components:
  * Google Account containing an automatically set-up Google Spreadsheet
  * Universal Remote Controller (URC)

### Alexa Skill

* Takes care of activation and hands off control to AWS Lambda
* Links to user's Google Account

### AWS Lambda

* Receives and keeps websocket connection from each URC and sends commands to the URCs
* Reads configuration data from user's Google Spreadsheet

TODO: this needs to be worked out further, following 

### Google Account with Spreadsheet

The Spreadsheet contains all necessary configuration data to control the user's specific environment:
* URCID
* IR/RF transmission sequences grouped by appliance
* mappings of commands to transmission sequences, e.g. "Turn On TV" --> specific IR transmission sequence to turn on TV set

### Universal Remote Controller

* ESP8266 system with IR and RF transmitter
* has a baked-in unique URCID
* upon startup establishes websocket connection to AWS Lambda endpoint, passing its URCID
* receives and executes commands from AWS Lambda

## Consequences

What becomes easier or more difficult to do and any risks introduced by the change that will need to be mitigated.
