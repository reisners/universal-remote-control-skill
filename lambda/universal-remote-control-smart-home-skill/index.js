const { google } = require('googleapis');

const GOOGLE_SHEET_NAME = "UniversalRemoteControlData";

const AWS = require('aws-sdk');

AWS.config.update({region:'eu-west-1'});

const ddb = new AWS.DynamoDB.DocumentClient();

const { TABLE_NAME, ENDPOINT } = process.env;

exports.handler = function (request, context) {
    if (request.directive.header.namespace === 'Alexa.Discovery' && request.directive.header.name === 'Discover') {
        log("DEBUG:", "Discover request",  JSON.stringify(request));
        handleDiscovery(request, context, "");
    }
    else if (request.directive.header.namespace === 'Alexa.PowerController') {
        if (request.directive.header.name === 'TurnOn' || request.directive.header.name === 'TurnOff') {
            log("DEBUG:", "TurnOn or TurnOff Request", JSON.stringify(request));
            handlePowerControl(request, context);
        }
    }

    async function handleDiscovery(request, context) {
        log("DEBUG:","request=", JSON.stringify(request));
        let accessToken = request.directive.payload.scope.token;
        let oAuth2Client = createOAuthClient(accessToken);
        let attributes = await retrieveAttributes(oAuth2Client);
        let devices = await retrieveDevices(attributes.spreadsheetId, oAuth2Client);
        log("DEBUG:", "devices=", JSON.stringify(devices));
        
        var payload = {
            "endpoints": Object.entries(devices).map((entry) => {

                let endpointId = entry[0];
                let device = entry[1];
                let [urcid] = endpointId.split(":");
                let operations = Object.entries(device);
                let operation0 = operations[0][1];
                let interface = operation0.category; // all rows for the same device must have identical URCID, Device, Display Category and Category values, so we pick the first one here
                let displayCategory = operation0.displayCategory;
                let deviceName = operation0.device;
                return {
                    "endpointId": endpointId,
                    "manufacturerName": "Universal Remote Control",
                    "friendlyName": deviceName,
                    "description": deviceName,
                    "displayCategories": [displayCategory],
                    "additionalAttributes": {
                        "manufacturer" : "Universal Remote Control",
                        "model" : "MK1",
                        "serialNumber": urcid,
                        "firmwareVersion" : "0.0",
                        "softwareVersion": "0.0",
                        "customIdentifier": urcid
                    },
                    "cookie": {},
                    "connections": [],
                    "capabilities":
                    [
                        {
                            "type": "AlexaInterface",
                            "interface": "Alexa",
                            "version": "3"
                        },
                        {
                            "type": "AlexaInterface",
                            "interface": interface,
                            "version": "3",
                        }
                    ]
                }}
            )
        };
        var header = request.directive.header;
        header.name = "Discover.Response";
        log("DEBUG:", "Discovery Response: ", JSON.stringify({ header: header, payload: payload }));
        context.succeed({ event: { header: header, payload: payload } });
    }

    function log(message, message1, message2) {
        console.log(message + message1 + message2);
    }

    async function handlePowerControl(request, context) {
        log("handlePowerController(request="+JSON.stringify(request, getCircularReplacer())+")")

        let requestMethod = request.directive.header.name;
        let messageId = request.directive.header.messageId;
        let responseHeader = Object.assign({}, request.directive.header);
        responseHeader.namespace = "Alexa";
        responseHeader.name = "Response";
        responseHeader.messageId = responseHeader.messageId + "-R";
        // get user token pass in request
        let accessToken = request.directive.endpoint.scope.token;
        let oAuth2Client = createOAuthClient(accessToken);
        let attributes = await retrieveAttributes(oAuth2Client);
        let devices = await retrieveDevices(attributes.spreadsheetId, oAuth2Client);

        let endpointId = request.directive.endpoint.endpointId;
        let [urcid, deviceName] = endpointId.split(":");

        //TODO: find entry in Google sheet matching endpointId=URCID:Device & requestMethod=Operation
        let operation = devices[endpointId][requestMethod];
        let channel = operation.channel;
        let parsedData = operation.data;

        //TODO: make URC return the powerResult value ("ON"/"OFF") and pass it back to Alexa
        var response;
        try {
            var powerResult = await executeCommmand(urcid, channel, parsedData);

            if (requestMethod === "TurnOn") {
    
                // Make the call to your device cloud for control
                // powerResult = stubControlFunctionToYourCloud(endpointId, token, request);
                powerResult = "ON";
            }
           else if (requestMethod === "TurnOff") {
                // Make the call to your device cloud for control and check for success
                // powerResult = stubControlFunctionToYourCloud(endpointId, token, request);
                powerResult = "OFF";
            }
            var contextResult = {
                "properties": [{
                    "namespace": "Alexa.PowerController",
                    "name": "powerState",
                    "value": powerResult,
                    "timeOfSample": "2017-09-03T16:20:50.52Z", //retrieve from result.
                    "uncertaintyInMilliseconds": 50
                }]
            };
            response = {
                context: contextResult,
                event: {
                    header: responseHeader,
                    endpoint: {
                        scope: {
                            type: "BearerToken",
                            token: accessToken
                        },
                        endpointId: endpointId
                    },
                    payload: {}
                }
            };
        } catch (err) {
            log("ERROR:", err);
            response = {
                event: {
                  header: {
                    namespace: "Alexa",
                    name: "ErrorResponse",
                    messageId: messageId,
                    payloadVersion: "3"
                  },
                  endpoint:{
                    endpointId: endpointId
                  },
                  payload: {
                    type: "ENDPOINT_UNREACHABLE",
                    message: err
                  }
                }
              }
        }
        log("DEBUG:", "Alexa.PowerController ", JSON.stringify(response));
        context.succeed(response);
    }

    function createOAuthClient(accessToken) {
        const oAuth2Client = new google.auth.OAuth2();
        oAuth2Client.setCredentials({access_token: accessToken});
        return oAuth2Client;
    }

    async function retrieveAttributes(oAuth2Client) {
        const drive = google.drive({
            version: 'v3',
            auth: oAuth2Client,
          });
          
        return readAttributesFromGoogleDrive(drive);
    }

    async function readAttributesFromGoogleDrive(drive) {
        var pageToken = null;
        return new Promise(async (resolve, reject) => {
            drive.files.list({
                q: "name='"+GOOGLE_SHEET_NAME+"'",
                fields: 'nextPageToken, files(id, name)',
                spaces: 'drive',
                pageToken: pageToken
            }, function (err, res) {
                if (err) {
                    reject(err);
                } else {
                    console.log("sheet(s) found on google drive: "+res.data.files.map(sheet => sheet.id));
                    resolve({spreadsheetId: res.data.files[0].id});
                }
            });    
        });
    }

    async function retrieveDevices(spreadsheetId, oAuth2Client) {
        let request = {
            spreadsheetId: spreadsheetId,
            range: 'A2:G',
            valueRenderOption: 'UNFORMATTED_VALUE',
            dateTimeRenderOption: 'FORMATTED_STRING',
            auth: oAuth2Client,
          };
        
        return new Promise( (resolve, reject) => {
            const sheets = google.sheets('v4');
            sheets.spreadsheets.values.get(request, function(err, response) {
                if (err) {
                    console.error(err);
                    reject(err);
                } else {
                    let devices = {};
                    log("DEBUG:", 'read spreadsheet: '+JSON.stringify(response, getCircularReplacer()));
                    response.data.values.forEach((row) => {
                        try {
                            let [urcid, device, displayCategory, category, operation, channel, jsonData] = row;
                            let endpointId = urcid + ":" + device;
                            try {
                                let data = JSON.parse(jsonData);
                                if (!devices[endpointId]) {
                                    devices[endpointId] = {};
                                }
                                if (devices[endpointId][operation]) {
                                    console.log("WARNING: duplicate entries for endpointId "+endpointId+" operation "+operation);
                                }
                                devices[endpointId][operation] = { urcid: urcid, device: device, displayCategory: displayCategory, category: category, channel: channel, data: data };    
                            } catch (parseError) {
                                console.log("ERROR: endpointId "+endpointId+" operation "+operation+" JSON data "+jsonData+" could not be parsed: "+parseError.message);
                            }
                        } catch (e) {
                            reject(e);
                        }
                    });
                    console.log('read devices: '+JSON.stringify(devices, getCircularReplacer()));
                    resolve(devices);
                }
            });
        });    
    }
    
    async function executeCommmand(urcid, channel, parsedData) {

        connectionData = await ddb.scan({ TableName: TABLE_NAME, ProjectionExpression: '#connectionId',
                                        ExpressionAttributeNames: {
                                            '#URCID': 'URCID',
                                            '#connectionId': 'connectionId'
                                        },
                                        ExpressionAttributeValues: {
                                        ":urcid": urcid
                                        }, 
                                        FilterExpression: "#URCID = :urcid", 
                                        }).promise();
        const apigwParams = {
            apiVersion: '2018-11-29',
            endpoint: ENDPOINT
        };
        
        console.log("API Gateway Parameters: "+JSON.stringify(apigwParams));
        
        const apigwManagementApi = new AWS.ApiGatewayManagementApi(apigwParams);
                
        const payload = { command: "SEND", channel: channel, data: parsedData };
        const payloadJson = JSON.stringify(payload);
        
        const postCalls = connectionData.Items.map(async ({ connectionId }) => {
            console.log("posting "+payloadJson+" to connectionId "+connectionId);
            try {
                return await apigwManagementApi.postToConnection({ ConnectionId: connectionId, Data: payloadJson}).promise();
            } catch (e) {
                if (e.statusCode === 410) {
                    console.log(`Found stale connection, deleting ${connectionId}`);
                    await ddb.delete({ TableName: TABLE_NAME, Key: { connectionId } }).promise();
                    return null;
                } else {
                    throw e;
                }
            }
        })
        .filter(postCall => !!postCall);

        if (postCalls.length == 0) {
            throw new Error("no active connections found");
        }
        
        await Promise.all(postCalls);
    }
}

const getCircularReplacer = () => {
    const seen = new WeakSet();
    return (key, value) => {
      if (typeof value === "object" && value !== null) {
        if (seen.has(value)) {
          return;
        }
        seen.add(value);
      }
      return value;
    };
  };

  
