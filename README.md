# Homebridge Plugin for PMS7003

This is a Homebridge plugin for PMS7003 dust/particulate matter sensor, working on the Raspberry Pi 3 or Zero.

It uses read() and write() syscalls on the UART devices exposed by Linux's filesystem, and epoll to watch for messages on the UART file descriptor with a timeout.

## Configuration

The PMS7003 must be connected to pins BCM14/BCM15 on a Raspberry Pi 3/Zero.

| Field name           | Description                                                                | Type / Unit    | Default value       | Required? |
| -------------------- |:-------------------------------------------------------------------------- |:--------------:|:-------------------:|:---------:|
| name                 | Name of the accessory                                                      | string         | —                   | Y         |
| enableFakeGato       | Enable storing data in Eve Home app                                        | bool           | false               | N         |
| fakeGatoStoragePath  | Path to store data for Eve Home app                                        | string         | (fakeGato default)  | N         |
| enableMQTT           | Enable sending data to MQTT server                                         | bool           | false               | N         |
| mqttConfig           | Object containing some config for MQTT                                     | object         | —                   | N         |
| showTemperatureTile  | Enable showing temperature tile in Home that represents PM10               | bool           | false               | N         |
| showHumidityTile     | Enable showing humidity tile in Home that represents 0.3µm-0.5µm particles | bool           | false               | N         |

If `showTemperatureTile` is `true`, a temperature tile will show up in Home with the PM10 in µg/m3.
If `showHumidityTile` is `true`, a humidity tile will show up in Home with the number of particles of size 0.3-0.5µm in 0.1L of air, divided by 100 (i.e. 4% humidity = 400 particles).

The mqttConfig object is **only required if enableMQTT is true**, and is defined as follows:

| Field name  | Description                                                         | Type / Unit  | Default value          | Required? |
| ----------- |:--------------------------------------------------------------------|:------------:|:----------------------:|:---------:|
| url         | URL of the MQTT server, must start with mqtt://                     | string       | —                      | Y         |
| PM1_0STopic | MQTT topic to which standard PM1.0 data is sent                     | string       | PMS7003/PM1.0 Standard | N         |
| PM2_5STopic | MQTT topic to which standard PM2.5 data is sent                     | string       | PMS7003/PM2.5 Standard | N         |
| PM10STopic  | MQTT topic to which standard PM10 data is sent                      | string       | PMS7003/PM10 Standard  | N         |
| PM1_0Topic  | MQTT topic to which current PM1.0 data is sent                      | string       | PMS7003/PM1.0          | N         |
| PM2_5Topic  | MQTT topic to which current PM2.5 data is sent                      | string       | PMS7003/PM2.5          | N         |
| PM10Topic   | MQTT topic to which current PM10 data is sent                       | string       | PMS7003/PM10           | N         |
| B0_3Topic   | MQTT topic to which number of particles of size 0.3-0.5µm is sent   | string       | PMS7003/0.3            | N         |
| B0_5Topic   | MQTT topic to which number of particles of size 0.5-1.0µm is sent   | string       | PMS7003/0.5            | N         |
| B1_0Topic   | MQTT topic to which number of particles of size 1.0-2.5µm is sent   | string       | PMS7003/1.0            | N         |
| B2_5Topic   | MQTT topic to which number of particles of size 2.5-5.0µm is sent   | string       | PMS7003/2.5            | N         |
| B5_0Topic   | MQTT topic to which number of particles of size 5.0-10µm is sent    | string       | PMS7003/5.0            | N         |
| B10Topic    | MQTT topic to which number of particles of size >10µm is sent       | string       | PMS7003/10             | N         |

The difference between standard/current particle concentrations is that standard concentrations are scaled for ICAO standard atmosphere at sea level (i.e. 288.15K and 1013.25hPa).

### Example Configuration

```
{
  "bridge": {
    "name": "Homebridge",
    "username": "XXXX",
    "port": XXXX
  },

  "accessories": [
    {
        "accessory": "PMS7003",
        "name": "Office PMS7003",
        "enableFakeGato": true,
        "showTemperatureTile": true,
        "showHumidityTile": true,
        "enableMQTT": true,
        "mqtt": {
            "url": "mqtt://192.168.0.38",
            "PM1_0STopic": "pms7003/PM1.0 Standard",
            "PM2_5STopic": "pms7003/PM2.5 Standard",
            "PM10STopic": "pms7003/PM10 Standard",
            "PM1_0Topic": "pms7003/PM1.0",
            "PM2_5Topic": "pms7003/PM2.5",
            "PM10Topic": "pms7003/PM10",
            "B0_3Topic": "pms7003/0.3",
            "B0_5Topic": "pms7003/0.5",
            "B1_0Topic": "pms7003/1.0",
            "B2_5Topic": "pms7003/2.5",
            "B5_0Topic": "pms7003/5.0",
            "B10Topic": "pms7003/10"
        }
    }
  ]
}
```

## Project Layout

- All things required by Node are located at the root of the repository (i.e. package.json and index.js).
- The rest of the code is in `src`, further split up by language.
  - `c` contains the C code that runs on the device to communicate with the sensor. It also contains a simple C program that communicates with the sensor.
  - `binding` contains the C++ code using node-addon-api to communicate between C and the Node.js runtime.
  - `js` contains a simple project that tests that the binding between C/Node.js is correctly working. It also contains a characteristic that enables Eve to keep a historical graph of air quality.
