# Steps to build

An execution role (`universal-remote-control-smart-home-skill-role-t83je30f`) has be created manually up front with following permissions:
* AmazonDynamoDBFullAccess
* AmazonAPIGatewayInvokeFullAccess
* ```{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": "logs:CreateLogGroup",
            "Resource": "arn:aws:logs:eu-west-1:442943444087:*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "logs:CreateLogStream",
                "logs:PutLogEvents"
            ],
            "Resource": [
                "arn:aws:logs:eu-west-1:442943444087:log-group:/aws/lambda/universal-remote-control-smart-home-skill:*"
            ]
        }
    ]
}```

Then to create the lambda function enter
```claudia --profile claudia create --region eu-west-1 --handler index.handler --role universal-remote-control-smart-home-skill-role-t83je30f --set-env-from-json env.json```

To update the existing lambda function enter
```claudia --profile claudia update --set-env-from-json env.json
```
