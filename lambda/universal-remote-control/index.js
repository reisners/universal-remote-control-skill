const { google } = require('googleapis');

const GOOGLE_SHEET_NAME = "UniversalRemoteControlData";

const DISPLAY_CATEGORIES = {
    "Alexa.PowerControl": "SWITCH",
    "Alexa.ChannelController": "TV"
};

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
        
        log("DEBUG:", "spreadsheetId=", attributes.spreadsheetId);

        var payload = {
            "endpoints": Object.entries(devices).map((entry) => {

                let deviceName = entry[0];
                let interface = Object.entries(entry[1])[0].category; // all rows for the same device must have identical category, so we pick the first one here
                return {
                    "endpointId": "urc:"+deviceName,
                    "manufacturerName": "Universal Remote Control",
                    "friendlyName": deviceName,
                    "description": deviceName,
                    "displayCategories": [DISPLAY_CATEGORIES[interface]],
                    "cookie": entry[1],
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
        log("DEBUG", "Discovery Response: ", JSON.stringify({ header: header, payload: payload }));
        context.succeed({ event: { header: header, payload: payload } });
    }

    function log(message, message1, message2) {
        console.log(message + message1 + message2);
    }

    function handlePowerControl(request, context) {
        // get device ID passed in during discovery
        var requestMethod = request.directive.header.name;
        var responseHeader = request.directive.header;
        responseHeader.namespace = "Alexa";
        responseHeader.name = "Response";
        responseHeader.messageId = responseHeader.messageId + "-R";
        // get user token pass in request
        var requestToken = request.directive.endpoint.scope.token;
        var powerResult;

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
        var response = {
            context: contextResult,
            event: {
                header: responseHeader,
                endpoint: {
                    scope: {
                        type: "BearerToken",
                        token: requestToken
                    },
                    endpointId: "demo_id"
                },
                payload: {}
            }
        };
        log("DEBUG", "Alexa.PowerController ", JSON.stringify(response));
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
                    resolve({spreadsheetId: res.data.files[0].id});
                }
            });    
        });
    }

    async function retrieveDevices(spreadsheetId, oAuth2Client) {
        let request = {
            spreadsheetId: spreadsheetId,
            range: 'A2:D',
            valueRenderOption: 'UNFORMATTED_VALUE',
            dateTimeRenderOption: 'FORMATTED_STRING',
            auth: oAuth2Client,
          };
        
        return new Promise( (resolve, reject) => {
            const sheets = google.sheets('v4');
            sheets.spreadsheets.values.get(request, function(err, response) {
                if (err) {
                    console.log('an error occured');
                    console.error(err);
                    reject(err);
                } else {
                    let devices = {};
                    //console.log('read spreadsheet: '+JSON.stringify(response, getCircularReplacer()));
                    response.data.values.forEach((row) => {
                        let [device, category, operation, channel, data] = row;
                        if (!devices[device]) {
                            devices[device] = {};
                        }
                        if (devices[device][operation]) {
                            console.log("WARNING: duplicate entries for device "+device+" operation "+operation);
                        }
                        devices[device][operation] = { category: category, channel: channel, data: data };
                    });
                    console.log('read devices: '+JSON.stringify(devices, getCircularReplacer()));
                    resolve(devices);
                }
            });
        });    
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
