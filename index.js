const PMS7003 = require('bindings')('homebridge-pms7003');

const inherits = require('util').inherits;
const moment = require('moment'); // Time formatting
const mqtt = require('mqtt'); // MQTT client
const os = require('os'); // Hostname

var Service, Characteristic;
var CustomCharacteristic;
var FakeGatoHistoryService;

module.exports = homebridge => {
  Service = homebridge.hap.Service;
  Characteristic = homebridge.hap.Characteristic;
  FakeGatoHistoryService = require('fakegato-history')(homebridge);
  CustomCharacteristic = require('./src/js/customcharacteristic.js')(homebridge);

  homebridge.registerAccessory("homebridge-pms7003", "PMS7003", PMS7003Accessory);
}

function PMS7003Accessory(log, config) {
  // Load configuration from files
  this.log = log;
  this.displayName = config['name'];
  this.showTemperatureTile = typeof config['showTemperatureTile'] === 'undefined' ? true : config['showTemperatureTile'];
  this.showHumidityTile = typeof config['showHumidityTile'] === 'undefined' ? true : config['showHumidityTile'];
  this.enableFakeGato = config['enableFakeGato'] || false;
  this.fakeGatoStoragePath = config['fakeGatoStoragePath'];
  this.enableMQTT = config['enableMQTT'] || false;
  this.mqttConfig = config['mqtt'];

  // Internal variables to keep track of current particle concentrations
  this._currentPM2_5 = null;
  this._2_5Samples = [];
  this._2_5CumSum = 0;
  this._2_5Counter = 0;
  this._currentPM10 = null;
  this._10Samples = [];
  this._10CumSum = 0;
  this._10Counter = 0;
  this._currentB0_3 = null;
  this._B0_3Samples = [];
  this._B0_3CumSum = 0;
  this._B0_3Counter = 0;
  this._otherCounter = 0;

  // Services
  let informationService = new Service.AccessoryInformation();
  informationService
    .setCharacteristic(Characteristic.Manufacturer, "Plantower")
    .setCharacteristic(Characteristic.Model, "PMS7003")
    .setCharacteristic(Characteristic.SerialNumber, `${os.hostname}-0`)
    .setCharacteristic(Characteristic.FirmwareRevision, require('./package.json').version);

  let airQualityService = new Service.AirQualitySensor();
  airQualityService.addCharacteristic(Characteristic.PM10Density)
  airQualityService.addCharacteristic(Characteristic.PM2_5Density);
  airQualityService.addCharacteristic(CustomCharacteristic.EveAirQuality);
  airQualityService.addCharacteristic(CustomCharacteristic.EveAirQualityUnknown);

  let temperatureService = new Service.TemperatureSensor("PM10", "pm10");
  let humidityService = new Service.HumiditySensor("0.3-0.5µm bucket", "bucket0_3");

  this.informationService = informationService;
  this.airQualityService = airQualityService;
  this.temperatureService = temperatureService;
  this.humidityService = humidityService;

  // Start FakeGato for logging historical data
  if (this.enableFakeGato) {
    this.fakeGatoHistoryService = new FakeGatoHistoryService("room", this, {
      storage: 'fs',
      folder: this.fakeGatoStoragePath
    });
  }

  // Set up MQTT client
  if (this.enableMQTT) {
    this.setUpMQTT();
  }

  // Periodically update the values
  this.setupPMS7003();
  this.refreshData();
  setInterval(() => this.refreshData(), 1000);
}

// Error checking and averaging when saving PM2.5 and PM10
Object.defineProperty(PMS7003Accessory.prototype, "PM2_5", {
  set: function(PM2_5Reading) {
    if (PM2_5Reading > 1000 || PM2_5Reading < 0) {
      this.log(`Error: PM2.5 reading out of range: ` +
               `Reading: ${PM2_5Reading}, Current: ${this._currentPM2_5}`);
      return;
    }

    if (this._2_5Samples.length == 30) {
      let firstSample = this._2_5Samples.shift();
      this._2_5CumSum -= firstSample;
    }
    this._2_5Samples.push(PM2_5Reading);
    this._2_5CumSum += PM2_5Reading;
    this._2_5Counter++;

    // Update current PM2.5 value, and publish to MQTT/FakeGato once every 30 seconds
    if (this._2_5Counter == 30) {
      this._2_5Counter = 0;
      this._currentPM2_5 = this._2_5CumSum / 30;
      this.log(`PM2.5: ${this._currentPM2_5}`);

      this.airQualityService.getCharacteristic(Characteristic.PM2_5Density)
        .updateValue(this._currentPM2_5);
      this.airQualityService.getCharacteristic(CustomCharacteristic.EveAirQuality)
        .updateValue(this._currentPM2_5);
      if (this._currentPM2_5 <= 15) {
        this.airQualityService.getCharacteristic(Characteristic.AirQuality)
          .updateValue(1);
      } else if (this._currentPM2_5 > 15 && this._currentPM2_5 <= 40) {
        this.airQualityService.getCharacteristic(Characteristic.AirQuality)
          .updateValue(2);
      } else if (this._currentPM2_5 > 40 && this._currentPM2_5 <= 65) {
        this.airQualityService.getCharacteristic(Characteristic.AirQuality)
          .updateValue(3);
      } else if (this._currentPM2_5 > 65 && this._currentPM2_5 <= 150) {
        this.airQualityService.getCharacteristic(Characteristic.AirQuality)
          .updateValue(4);
      } else {
        this.airQualityService.getCharacteristic(Characteristic.AirQuality)
          .updateValue(5);
      }
 
      if (this.enableFakeGato) {
        this.fakeGatoHistoryService.addEntry({
          time: moment().unix(),
          ppm: this._currentPM2_5 + 450, // Eve has a 450ppm floor for graphing, so add 450 to compensate 
        });
      }
 
      if (this.enableMQTT) {
        this.publishToMQTT(this.PM2_5Topic, this._currentPM2_5);
      }
    }
  },

  get: function() {
    return this._currentPM2_5;
  }
});

Object.defineProperty(PMS7003Accessory.prototype, "PM10", {
  set: function(PM10Reading) {
    if (PM10Reading > 1000 || PM10Reading < 0) {
      this.log(`Error: PM10 reading out of range: ` +
               `Reading: ${PM10Reading}, Current: ${this._currentPM10}`);
      return;
    }

    // Calculate running average of PM10 over the last 30 samples
    this._10Counter++;
    if (this._10Samples.length == 30) {
      let firstSample = this._10Samples.shift();
      this._10CumSum -= firstSample;
    }
    this._10Samples.push(PM10Reading);
    this._10CumSum += PM10Reading;

    // Publish TVOC to MQTT every 30 seconds
    if (this._10Counter == 30) {
      this._10Counter = 0;
      this._currentPM10 = this._10CumSum / 30;
      this.log(`PM10: ${this._currentPM10}`);

      this.airQualityService.getCharacteristic(Characteristic.PM10Density)
       .updateValue(this._currentPM10);
 
      this.temperatureService.getCharacteristic(Characteristic.CurrentTemperature)
        .updateValue(this._currentPM10);

      if (this.enableFakeGato) {
        this.fakeGatoHistoryService.addEntry({
          time: moment().unix(),
          temp: this._currentPM10,
        });
      }
      
      if (this.enableMQTT) {
        this.publishToMQTT(this.PM10Topic, this._currentPM10);
      }
    }
  },

  get: function() {
    return this._currentPM10;
  }
});

Object.defineProperty(PMS7003Accessory.prototype, "B0_3", {
  set: function(B0_3Reading) {
    if (B0_3Reading > 10000 || B0_3Reading < 0) {
      this.log(`Error: 0.3-0.5µm bucket reading out of range: ` +
               `Reading: ${B0_3Reading}, Current: ${this._currentB0_3}`);
      return;
    }

    if (this._B0_3Samples.length == 30) {
      let firstSample = this._B0_3Samples.shift();
      this._B0_3CumSum -= firstSample;
    }
    this._B0_3Samples.push(B0_3Reading);
    this._B0_3CumSum += B0_3Reading;
    this._B0_3Counter++;

    // Update current 0.3µm bucket value, and publish to MQTT/FakeGato once every 30 seconds
    if (this._B0_3Counter == 30) {
      this._B0_3Counter = 0;
      this._currentB0_3 = this._B0_3CumSum / 30;
      this.log(`Particles >0.3µm and <0.5µm: ${this._currentB0_3}`);

      this.humidityService.getCharacteristic(Characteristic.CurrentRelativeHumidity)
        .updateValue(this._currentB0_3 / 10000 * 100);
 
      if (this.enableFakeGato) {
        this.fakeGatoHistoryService.addEntry({
          time: moment().unix(),
          humidity: this._currentB0_3 / 10000 * 100, // Assume maximum 10000 particles, scale to between 0 and 100% for humidity 
        });
      }
 
      if (this.enableMQTT) {
        this.publishToMQTT(this.B0_3Topic, this._currentB0_3);
      }
    }
  },

  get: function() {
    return this._currentB0_3;
  }
});

// Sets up MQTT client based on config loaded in constructor
PMS7003Accessory.prototype.setUpMQTT = function() {
  if (!this.enableMQTT) {
    this.log.info("MQTT not enabled");
    return;
  }

  if (!this.mqttConfig) {
    this.log.error("No MQTT config found");
    return;
  }

  this.mqttUrl = this.mqttConfig.url;
  this.PM1_0STopic = this.mqttConfig.PM1_0STopic || 'PMS7003/PM1.0 Standard';
  this.PM2_5STopic = this.mqttConfig.PM2_5STopic || 'PMS7003/PM2.5 Standard';
  this.PM10STopic = this.mqttConfig.PM10STopic || 'PMS7003/PM10 Standard';
  this.PM1_0Topic = this.mqttConfig.PM1_0Topic || 'PMS7003/PM1.0';
  this.PM2_5Topic = this.mqttConfig.PM2_5Topic || 'PMS7003/PM2.5';
  this.PM10Topic = this.mqttConfig.PM10Topic || 'PMS7003/PM10';
  this.B0_3Topic = this.mqttConfig.B0_3Topic || 'PMS7003/0.3'; 
  this.B0_5Topic = this.mqttConfig.B0_5Topic || 'PMS7003/0.5'; 
  this.B1_0Topic = this.mqttConfig.B1_0Topic || 'PMS7003/1.0'; 
  this.B2_5Topic = this.mqttConfig.B2_5Topic || 'PMS7003/2.5'; 
  this.B5_0Topic = this.mqttConfig.B5_0Topic || 'PMS7003/5.0'; 
  this.B10Topic = this.mqttConfig.B10Topic || 'PMS7003/10'; 

  this.mqttClient = mqtt.connect(this.mqttUrl);
  this.mqttClient.on("connect", () => {
    this.log(`MQTT client connected to ${this.mqttUrl}`);
  });
  this.mqttClient.on("error", (err) => {
    this.log(`MQTT client error: ${err}`);
    client.end();
  });
}

// Sends data to MQTT broker; must have called setupMQTT() previously
PMS7003Accessory.prototype.publishToMQTT = function(topic, value) {
  if (!this.mqttClient.connected || !topic) {
    this.log.error("MQTT client not connected, or no topic or value for MQTT");
    return;
  }
  this.mqttClient.publish(topic, String(value));
}

// Set up sensor
PMS7003Accessory.prototype.setupPMS7003 = function() {
  let data = PMS7003.init();
  if (data.hasOwnProperty('errcode')) {
    this.log(`Error: ${data.errmsg}`);
  }
}

// Read particle concentrations from sensor
PMS7003Accessory.prototype.refreshData = function() {
  let data = PMS7003.read(1000);

  if (data.hasOwnProperty('errcode')) {
    this.log(`Error: ${data.errmsg}`);
    // Updating a value with Error class sets status in HomeKit to 'Not responding'
    this.airQualityService.getCharacteristic(Characteristic.PM10Density)
      .updateValue(Error(data.errmsg));
    this.airQualityService.getCharacteristic(Characteristic.PM2_5Density)
      .updateValue(Error(data.errmsg));
    this.temperatureService.getCharacteristic(Characteristic.CurrentTemperature)
      .updateValue(Error(data.errmsg));
    this.humidityService.getCharacteristic(Characteristic.CurrentRelativeHumidity)
      .updateValue(Error(data.errmsg));
    return;
  }
  
  this.log.debug(`Read: PM2_5: ${data.pm2_5}µg/m3, PM10: ${data.pm10}µg/m3`); 
  this.PM2_5 = data.pm2_5;
  this.PM10 = data.pm10;
  this.B0_3 = data.bucket0_3;

  // Publish the other values to MQTT if required
  this._otherCounter++;
  if (this.enableMQTT && this._otherCounter == 30) {
    this._otherCounter = 0;
    this.publishToMQTT(this.PM1_0STopic, data.pm1_s);
    this.publishToMQTT(this.PM2_5STopic, data.pm2_5_s);
    this.publishToMQTT(this.PM10STopic, data.pm10_s);
    this.publishToMQTT(this.PM1_0Topic, data.pm1_0);
    this.publishToMQTT(this.B0_5Topic, data.bucket0_5);
    this.publishToMQTT(this.B1_0Topic, data.bucket1_0);
    this.publishToMQTT(this.B2_5Topic, data.bucket2_5);
    this.publishToMQTT(this.B5_0Topic, data.bucket5_0);
    this.publishToMQTT(this.B10Topic, data.bucket10);
  }
}

PMS7003Accessory.prototype.getServices = function() {
  if (this.showTemperatureTile) {
    if (this.showHumidityTile) {
      return [this.informationService,
              this.airQualityService,
              this.temperatureService,
              this.humidityService,
              this.fakeGatoHistoryService];
    } else {
      return [this.informationService,
              this.airQualityService,
              this.temperatureService,
              this.fakeGatoHistoryService];
    }
  } else {
    if (this.showHumidityTile) {
      return [this.informationService,
              this.airQualityService,
              this.humidityService,
              this.fakeGatoHistoryService];
    } else {
      return [this.informationService,
              this.airQualityService,
              this.fakeGatoHistoryService];
    }
  }
}

