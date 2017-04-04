import falcon
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

mqttClient = AWSIoTMQTTClient("im920-arduino-yun-gps")
mqttClient.configureEndpoint("xxxxxxxxxxxxxx.iot.ap-northeast-1.amazonaws.com", 8883)
mqttClient.configureCredentials("root-CA.crt", "private.pem.key", "certificate.pem.crt")
mqttClient.configureOfflinePublishQueueing(-1)
mqttClient.configureDrainingFrequency(2)
mqttClient.configureConnectDisconnectTimeout(10)
mqttClient.configureMQTTOperationTimeout(5)


class PublishResource(object):

    def on_post(self, req, resp):
        if req.host == "localhost":
            mqttClient.publish("im920/location", req.stream.read(), 1)
            resp.body = "OK"
        else:
            resp.body = "NG"

app = falcon.API()
app.add_route("/publish", PublishResource())

if __name__ == "__main__":
    mqttClient.connect()
    from wsgiref import simple_server
    httpd = simple_server.make_server("127.0.0.1", 8000, app)
    httpd.serve_forever()
